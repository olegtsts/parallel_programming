#include <memory>
#include <atomic>
#include <future>
#include <thread>
#include <vector>
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
    template <typename F>
    FunctionWrapper(F&& f)
    : function_impl(new ImplType<F>(std::move(f)))
    {}
    void operator()() {
        function_impl->Call();
    }
    FunctionWrapper() = delete;

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

class ThreadPool {
private:
    void WorkerThread() {
        while (!done) {
            RunPendingTask();
        }
    }
public:
    template <typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type> Submit(FunctionType f) {
        typedef typename std::result_of<FunctionType()>::type TResult;
        std::packaged_task<TResult()> task(std::move(f));
        std::future<TResult> res(task.get_future());
        work_queue.push(std::move(task));
        return res;
    }
    ThreadPool()
    : done(false)
    , joiner(threads)
    {
        const size_t thread_count = std::thread::hardware_concurrency();
        try {
            for (size_t i = 0; i < thread_count; ++i) {
                threads.push_back(std::thread(&ThreadPool::WorkerThread, this));
            }
        } catch (...) {
            done = true;
            throw;
        }
    }
    ~ThreadPool() {
        done = true;
    }
    void RunPendingTask() {
        std::unique_ptr<FunctionWrapper> ptr = work_queue.pop();
        if (ptr != nullptr) {
            ptr->operator()();
        } else {
            std::this_thread::yield();
        }
    }
private:
    std::atomic<bool> done;
    LockFreeQueue<FunctionWrapper> work_queue;
    std::vector<std::thread> threads;
    JoinThreads joiner;
};
int thread_task_int() {
    std::stringstream ss;
    ss << "Hello, i am thread " << std::this_thread::get_id() << "\n";
    std::cout << ss.str();
    return 1;
}
double thread_task_double() {
    std::stringstream ss;
    ss << "Hello, i am thread " << std::this_thread::get_id() << "\n";
    std::cout << ss.str();
    return 1.1;
}
int main() {
    ThreadPool thread_pool;
    std::cout << "Start\n";
    auto first_result = thread_pool.Submit(thread_task_int);
    auto second_result = thread_pool.Submit(thread_task_double);
    std::cout << "Result of int task: " << first_result.get() << "\n";
    std::cout << "Result of double task: " << second_result.get() << "\n";
    std::cout << "Stop\n";
    return 0;
}
