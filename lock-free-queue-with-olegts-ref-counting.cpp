#include <atomic>
#include <memory>
#include <future>
#include <thread>
#include <vector>
#include <functional>
#include <iostream>
#include <cassert>

std::atomic<int> ref_counter(0);

template <typename T>
class LockFreeQueue {
private:
    struct Node;

    struct CountedNodePtr {
        long long int external_count = 0;
        Node* ptr = nullptr;
    };

    struct NodeCounter {
        signed internal_count:30;
        unsigned external_counters:2;
    };
    struct Node {
        std::atomic<T*> data;
        std::atomic<NodeCounter> count;
        std::atomic<CountedNodePtr> next;

        Node()
        : data(nullptr)
        {
            count.store(NodeCounter{0, 3});
            next.store(CountedNodePtr{0, nullptr});
        }
    };

    static void RegisterCreatedPointer(Node* pointer) {
        ++ref_counter;
    }
    static void RegisterDeletedPointer(Node* pointer) {
        --ref_counter;
    }

    struct CountedNodePtrCopyGuard {
    public:
        CountedNodePtrCopyGuard(std::atomic<CountedNodePtr>& counter) {
            copied_counter = counter.load();
            CountedNodePtr new_counter;
            do {
                if (copied_counter.ptr == nullptr) {
                    return;
                }
                new_counter = copied_counter;
                ++new_counter.external_count;
            } while (!counter.compare_exchange_strong(copied_counter, new_counter,
                                                      std::memory_order_acquire,
                                                      std::memory_order_relaxed));
            copied_counter.external_count = new_counter.external_count;
        }

        CountedNodePtrCopyGuard& operator=(const CountedNodePtrCopyGuard&) = delete;
        CountedNodePtrCopyGuard(const CountedNodePtrCopyGuard&) = delete;

        CountedNodePtr GetCopy2() {
            return copied_counter;
        }

        ~CountedNodePtrCopyGuard() {
            if (copied_counter.ptr != nullptr) {
                NodeCounter old_counter = copied_counter.ptr->count.load(std::memory_order_relaxed);
                NodeCounter new_counter;
                do {
                    new_counter = old_counter;
                    --new_counter.internal_count;
                } while (!copied_counter.ptr->count.compare_exchange_strong(old_counter, new_counter,
                            std::memory_order_acquire, std::memory_order_relaxed));
                if (!new_counter.internal_count && !new_counter.external_counters) {
                    delete copied_counter.ptr;
                    RegisterDeletedPointer(copied_counter.ptr);
                }
            }
        }
    private:
        CountedNodePtr copied_counter;
    };

    static void FreeExternalCounter(CountedNodePtr old_node_ptr) {
        Node* ptr = old_node_ptr.ptr;
        int count_increase = old_node_ptr.external_count - 1;
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
            RegisterDeletedPointer(ptr);
        }
    }

    void SetNewTail(CountedNodePtr old_tail, CountedNodePtr new_tail) {
        Node* const current_tail_ptr = old_tail.ptr;
        new_tail.external_count = 1;
        while (!tail.compare_exchange_weak(old_tail, new_tail) && old_tail.ptr == current_tail_ptr) {}
        if (old_tail.ptr == current_tail_ptr) {
            FreeExternalCounter(old_tail);
        }
    }
public:
    LockFreeQueue() {
        CountedNodePtr empty_node;
        empty_node.ptr = new Node();
        RegisterCreatedPointer(empty_node.ptr);
        empty_node.external_count = 1;
        head.store(empty_node);
        tail.store(empty_node);
        FreeExternalCounter(empty_node);
    }

    void Push(T new_value) {
        std::unique_ptr<T> new_data(new T(new_value));
        CountedNodePtr new_next;
        new_next.ptr = new Node();
        RegisterCreatedPointer(new_next.ptr);
        new_next.external_count = 1;

        while (true) {
            CountedNodePtrCopyGuard tail_copy(tail);
            CountedNodePtr old_tail = tail_copy.GetCopy2();
            T* old_data = nullptr;
            if (old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get())) {
                CountedNodePtr old_next{0, nullptr};
                if (!old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
                    delete new_next.ptr;
                    RegisterDeletedPointer(new_next.ptr);
                    new_next = old_next;
                }
                SetNewTail(old_tail, new_next);
                new_data.release();
                break;
            } else {
                CountedNodePtr old_next{0, nullptr};
                if (old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
                    old_next = new_next;
                    new_next.ptr = new Node();
                    RegisterCreatedPointer(new_next.ptr);
                }
                SetNewTail(old_tail, old_next);
            }
        }
    }

    std::unique_ptr<T> Pop() {
        while (true) {
            CountedNodePtrCopyGuard head_copy(head);
            CountedNodePtr old_head = head_copy.GetCopy2();
            Node * ptr = old_head.ptr;
            if (ptr == tail.load().ptr) {
                return std::unique_ptr<T>();
            }
            CountedNodePtrCopyGuard next_copy(ptr->next);
            CountedNodePtr old_next = next_copy.GetCopy2();
            if (old_next.ptr != nullptr) {
                CountedNodePtr new_head = old_next;
                new_head.external_count = 1;
                if (head.compare_exchange_strong(old_head, new_head)) {
                    T* res = ptr->data.exchange(nullptr);
                    while (!ptr->next.compare_exchange_weak(old_next, CountedNodePtr{1, nullptr})) {}
                    FreeExternalCounter(old_head);
                    FreeExternalCounter(old_next);
                    return std::unique_ptr<T> (res);
                }
            }
        }
    }
    ~LockFreeQueue() {
        while (true) {
            auto popped = Pop();
            if (!popped) {
                break;
            }
        }
        FreeExternalCounter(head.load());
        FreeExternalCounter(tail.load());
    }
private:
    std::atomic<CountedNodePtr> head;
    std::atomic<CountedNodePtr> tail;
};

void thread_push(const int number, LockFreeQueue<int>& queue, std::shared_future<void> wait_to_go) noexcept {
    wait_to_go.wait();
    for (size_t i = 0;i < 10000000; ++i) {
        queue.Push(number);
    }
}
void thread_pop(LockFreeQueue<int>& queue, std::shared_future<void> wait_to_go) noexcept {
    wait_to_go.wait();
    for (size_t i = 0; i < 10000000; ++i) {
        auto ptr = queue.Pop();
    }
}
int main() {
    std::cout << "Started\n";
    {
        LockFreeQueue<int> queue;
        std::promise<void> go;
        std::shared_future<void> wait_to_go = go.get_future();
        std::vector<std::thread> threads;
        for (size_t i = 0; i < 2; ++i) {
            threads.push_back(std::thread(thread_push, i + 100, std::ref(queue), wait_to_go));
        }
        for (size_t i = 0; i < 2; ++i) {
            threads.push_back(std::thread(thread_pop, std::ref(queue), wait_to_go));
        }
        go.set_value();
        for (auto& one_thread: threads) {
            one_thread.join();
        }
    };
    std::cout << "ref_counter = " << ref_counter << std::endl;
    assert(ref_counter == 0);
    std::cout << "Done\n";
    return 0;
}
