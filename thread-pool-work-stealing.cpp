#include <deque>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <future>
#include <iostream>
#include <sstream>

class JoinThreads {
public:
    explicit JoinThreads(std::vector<std::thread>& threads)
    : threads(threads)
    {}

    ~JoinThreads() {
        for (auto& cur_thread: threads) {
            if (cur_thread.joinable()) {
                cur_thread.join();
            }
        }
    }
private:
    std::vector<std::thread>& threads;
};

template <typename T>
class LockFreeQueue {
private:
    struct Node;
    struct CountedNodePtr {
        long long int external_count;
        Node * ptr;
    };

    struct NodeCounter {
        unsigned internal_count:30;
        unsigned external_counters:2;
    };

    struct Node {
        std::atomic<T*> data;
        std::atomic<NodeCounter> count;
        std::atomic<CountedNodePtr> next;

        Node()
        : data(nullptr)
        {
            count.store(NodeCounter{0, 2});
            next.store(CountedNodePtr{0, nullptr});
        }

        void release_ref() {
            NodeCounter old_counter = count.load(std::memory_order_relaxed);
            NodeCounter new_counter;
            do {
                new_counter = old_counter;
                --new_counter.internal_count;
            } while (!count.compare_exchange_strong(old_counter, new_counter,
                        std::memory_order_acquire, std::memory_order_relaxed));
            if (!new_counter.internal_count && !new_counter.external_counters) {
                delete this;
            }
        }
    };

    void set_new_tail(CountedNodePtr& old_tail, const CountedNodePtr& new_tail) {
        Node* const current_tail_ptr = old_tail.ptr;
        while (!tail.compare_exchange_weak(old_tail, new_tail) && old_tail.ptr == current_tail_ptr) {}
        if (old_tail.ptr == current_tail_ptr) {
            free_external_counter(old_tail);
        } else {
            current_tail_ptr->release_ref();
        }
    }
public:
    LockFreeQueue() {
        CountedNodePtr new_next;
        new_next.ptr = new Node();
        new_next.external_count = 2;
        head.store(new_next);
        tail.store(new_next);
    }
    static void increase_external_count(std::atomic<CountedNodePtr>& counter,
            CountedNodePtr& old_counter) {
        CountedNodePtr new_counter;
        do {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while (!counter.compare_exchange_strong(old_counter, new_counter,
                    std::memory_order_acquire, std::memory_order_relaxed));
        old_counter.external_count = new_counter.external_count;
    }

    static void free_external_counter(CountedNodePtr& old_node_ptr) {
        Node* ptr = old_node_ptr.ptr;
        int count_increase = old_node_ptr.external_count - 2;
        NodeCounter old_counter = ptr->count.load(std::memory_order_relaxed);
        NodeCounter new_counter;
        do {
            new_counter = old_counter;
            --new_counter.external_counters;
            new_counter.internal_count += count_increase;
        } while (!ptr->count.compare_exchange_strong(old_counter, new_counter,
                std::memory_order_acquire, std::memory_order_relaxed));

        if (!new_counter.internal_count && !new_counter.external_counters) {
            delete ptr;
        }
    }

    void push(T&& new_value) {
        std::unique_ptr<T> new_data(new T(std::move(new_value)));
        CountedNodePtr new_next;
        new_next.ptr = new Node();
        new_next.external_count = 1;
        CountedNodePtr old_tail = tail.load();

        while (true) {
            increase_external_count(tail, old_tail);
            T* old_data = nullptr;
            if (old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get())) {
                CountedNodePtr old_next{0, nullptr};
                if (!old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
                    delete new_next.ptr;
                    new_next = old_next;
                }
                set_new_tail(old_tail, new_next);
                new_data.release();
                break;
            } else {
                CountedNodePtr old_next{0, nullptr};
                if (old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
                    old_next = new_next;
                    new_next.ptr = new Node();
                }
                set_new_tail(old_tail, old_next);
            }
        }
    }

    std::unique_ptr<T> pop() {
        CountedNodePtr old_head = head.load(std::memory_order_relaxed);
        while (true) {
            increase_external_count(head, old_head);
            Node * ptr = old_head.ptr;
            if (ptr == tail.load().ptr) {
                ptr->release_ref();
                return std::unique_ptr<T>();
            }
            CountedNodePtr next = ptr->next.load();
            if (head.compare_exchange_strong(old_head, next)) {
                T* res = ptr->data.exchange(nullptr);
                free_external_counter(old_head);
                return std::unique_ptr<T> (res);
            }
            ptr->release_ref();
        }
    }
private:
    std::atomic<CountedNodePtr> head;
    std::atomic<CountedNodePtr> tail;
};


class FunctionWrapper {
private:
    struct ImplBase {
        virtual void Call() = 0;
        virtual ~ImplBase() {}
    };
    template <typename F>
    struct ImplType : ImplBase {
        ImplType(F&& f)
        : f(std::move(f))
        {}
        void Call() {
            f();
        }
        F f;
    };
public:
    FunctionWrapper()
    : function_impl(nullptr)
    {}

    template <typename F>
    FunctionWrapper(F&& f)
    : function_impl(new ImplType<F>(std::move(f)))
    {}
    void operator()() {
        function_impl->Call();
    }

    FunctionWrapper(FunctionWrapper&& other)
    : function_impl(std::move(other.function_impl))
    {}
    FunctionWrapper& operator=(FunctionWrapper&& other) {
        function_impl = std::move(other.function_impl);
        return *this;
    }
    FunctionWrapper(const FunctionWrapper&) = delete;
    FunctionWrapper(FunctionWrapper&) = delete;
    FunctionWrapper& operator=(const FunctionWrapper&) = delete;
private:
    std::unique_ptr<ImplBase> function_impl;
};

class WorkStealingQueue {
private:
    typedef FunctionWrapper TData;
public:
    WorkStealingQueue()
    {}

    WorkStealingQueue(const WorkStealingQueue& other) = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue& other) = delete;

    void Push(TData&& data) {
        std::lock_guard<std::mutex> cur_lock(mu);
        data_queue.push_front(std::move(data));
    }
    bool Empty() const {
        std::lock_guard<std::mutex> cur_lock(mu);
        return data_queue.empty();
    }
    bool TryPop(TData& res) {
        std::lock_guard<std::mutex> cur_lock(mu);
        if (data_queue.empty()) {
            return false;
        }
        res = std::move(data_queue.front());
        data_queue.pop_front();
        return true;
    }
    bool TrySteal(TData& res) {
        std::lock_guard<std::mutex> cur_lock(mu);
        if (data_queue.empty()) {
            return false;
        }
        res = std::move(data_queue.back());
        data_queue.pop_back();
        return true;
    }
private:
    std::deque<TData> data_queue;
    mutable std::mutex mu;
};

class WorkStealingThreadPool {
private:
    typedef FunctionWrapper TTask;
    void WorkerThread(const size_t my_index_) {
        my_index = my_index_;
        local_work_queue = queues[my_index].get();
        while (!done) {
            RunPendingTask();
        }
    }
    bool PopTaskFromLocalQueue(TTask& task) {
        return local_work_queue && local_work_queue->TryPop(task);
    }
    bool PopTaskFromPoolQueue(TTask& task) {
        auto ptr = pool_work_queue.pop();
        if (ptr) {
            task = std::move(*ptr);
        }
        return ptr ? true : false;
    }
    bool PopTaskFromOtherThreadQueue(TTask& task) {
        for (size_t i = 0; i + 1 < queues.size(); ++i) {
            const size_t index = (my_index + i + 1) % queues.size();
            if (queues[index]->TrySteal(task)) {
                return true;
            }
        }
        return false;
    }
public:
    WorkStealingThreadPool()
    : done(false)
    , joiner(threads)
    {
        const size_t thread_count = std::thread::hardware_concurrency();
        try {
            for (size_t i = 0; i < thread_count; ++i) {
                queues.push_back(std::unique_ptr<WorkStealingQueue>(new WorkStealingQueue()));
            }
            for (size_t i = 0; i < thread_count; ++i) {
                threads.push_back(std::thread(&WorkStealingThreadPool::WorkerThread, this, i));
            }
        } catch (...) {
            done = true;
            throw;
        }
    }
    ~WorkStealingThreadPool() {
        done = true;
    }

    template <typename TFunction>
    std::future<typename std::result_of<TFunction()>::type> Submit(TFunction f) {
        typedef typename std::result_of<TFunction()>::type TResult;
        std::packaged_task<TResult()> task(f);
        std::future<TResult> res(task.get_future());
        if (local_work_queue) {
            local_work_queue->Push(std::move(task));
        } else {
            pool_work_queue.push(std::move(task));
        }
        return res;
    }
    void RunPendingTask() {
        TTask task;
        if (PopTaskFromLocalQueue(task) || PopTaskFromPoolQueue(task) || PopTaskFromOtherThreadQueue(task)) {
            task();
        } else {
            std::this_thread::yield();
        }
    }
private:
    std::atomic_bool done;
    LockFreeQueue<TTask> pool_work_queue;
    std::vector<std::unique_ptr<WorkStealingQueue>> queues;
    std::vector<std::thread> threads;
    JoinThreads joiner;
    static thread_local WorkStealingQueue* local_work_queue;
    static thread_local size_t my_index;
};

thread_local WorkStealingQueue* WorkStealingThreadPool::local_work_queue;
thread_local size_t WorkStealingThreadPool::my_index;

int get_result() {
    std::stringstream ss;
    ss << "This thread is " << std::this_thread::get_id() << std::endl;
    std::cout << ss.str();
    return 1;
}
int main() {
    WorkStealingThreadPool pool;
    std::vector<std::future<int>> results;
    for (size_t i = 0;i < 10; ++i) {
        results.push_back(pool.Submit(get_result));
    }
    std::cout << "Getting results\n";
    for (auto& res: results) {
        std::cout << res.get() << std::endl;
    }
    std::cout << "Done" << std::endl;
    return 0;
}
