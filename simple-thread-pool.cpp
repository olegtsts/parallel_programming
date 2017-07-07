#include <atomic>
#include <memory>
#include <functional>
#include <vector>
#include <thread>
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

    void push(T new_value) {
        std::unique_ptr<T> new_data(new T(new_value));
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

class ThreadPool {
private:
    void WorkerThread() {
        while (!done) {
            std::unique_ptr<std::function<void()>> task_pointer = work_queue.pop();
            if (task_pointer != nullptr) {
                task_pointer->operator()();
            } else {
                std::this_thread::yield();
            }
        }
    }
public:
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

    template <typename TFunction>
    void Submit(TFunction func) {
        work_queue.push(func);
    }
private:
    std::atomic<bool> done;
    LockFreeQueue<std::function<void()>> work_queue;
    std::vector<std::thread> threads;
    JoinThreads joiner;
};
void thread_task() {
    std::stringstream ss;
    ss << "Hello, i am thread " << std::this_thread::get_id() << "\n";
    std::cout << ss.str();
}
int main(){
    ThreadPool thread_pool;
    std::cout << "Start\n";
    for (size_t i = 0; i < 3; ++i) {
        thread_pool.Submit(thread_task);
    }
    std::cout << "Stop\n";
    return 0;
}
