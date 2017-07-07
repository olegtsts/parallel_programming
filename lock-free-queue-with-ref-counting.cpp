#include <iostream>
#include <atomic>
#include <memory>
#include <thread>
#include <sstream>
#include <functional>

template <typename T>
class LockFreeQueue {
private:
    struct Node;
    struct CountedNodePtr {
        int external_count = 0;
        Node* ptr = nullptr;
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

void thread_push(const int number, LockFreeQueue<int>& queue) noexcept {
    queue.push(number);
}
void thread_pop(LockFreeQueue<int>& queue) noexcept {
    auto ptr = queue.pop();
    std::stringstream ss;
    ss <<  (ptr.get() ? *ptr : 0) << std::endl;
    std::cout << ss.str();
}
int main() {
    std::cout << "Started\n";
    LockFreeQueue<int> queue;
    std::thread t1(thread_push, 1, std::ref(queue));
    std::thread t2(thread_push, 2, std::ref(queue));
    std::thread t3(thread_push, 3, std::ref(queue));
    std::thread t4(thread_pop, std::ref(queue));
    std::thread t5(thread_pop, std::ref(queue));
    std::thread t6(thread_pop, std::ref(queue));
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();
    std::cout << "Done\n";
    return 0;
}
