#include <iostream>
#include <atomic>
#include <memory>
#include <thread>
#include <sstream>

template <typename T>
class LockFreeStack {
public:
    LockFreeStack() {
        std::cout << "is stack lock free: " << std::boolalpha << head.is_lock_free() << std::endl;
    }
    void push(const T& data) {
        CountedNodePtr new_node;
        new_node.ptr = new Node(data);
        new_node.external_count = 1;
        new_node.ptr->next = head.load(std::memory_order_relaxed);
        while (!head.compare_exchange_weak(new_node.ptr->next, new_node,
                    std::memory_order_release, std::memory_order_relaxed)) {}
    }
    std::shared_ptr<T> pop() {
        CountedNodePtr old_head = head.load(std::memory_order_relaxed);
        while (true) {
            increase_head_count(old_head);
            Node* ptr = old_head.ptr;
            if (!ptr) {
                return std::shared_ptr<T>();
            }
            if (head.compare_exchange_strong(old_head, ptr->next, std::memory_order_relaxed, std::memory_order_relaxed)) {
                std::shared_ptr<T> res;
                res.swap(ptr->data);
                int count_increase = old_head.external_count - 2;
                if (ptr->internal_count.fetch_add(count_increase, std::memory_order_release) == -count_increase) {
                    delete ptr;
                }
                return res;
            } else if (ptr->internal_count.fetch_sub(1, std::memory_order_relaxed) == 1) {
                ptr->internal_count.load(std::memory_order_acquire);
                delete ptr;
            }
        }
    }
    ~LockFreeStack() {
        while(pop()) {}
    }
private:
    struct Node;
    struct CountedNodePtr {
        int external_count = 0;
        Node* ptr = nullptr;
    };
    struct Node {
        std::shared_ptr<T> data;
        std::atomic<int> internal_count;
        CountedNodePtr next;

        Node(const T& data)
        : data(std::make_shared<T>(data))
        , internal_count(0)
        {}
    };
    void increase_head_count(CountedNodePtr& old_counter) {
        CountedNodePtr new_counter;
        do {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while (!head.compare_exchange_strong(old_counter, new_counter, std::memory_order_acquire, std::memory_order_relaxed));
        old_counter.external_count = new_counter.external_count;
    }
    std::atomic<CountedNodePtr> head;
};
int main() {
    LockFreeStack<int> stack;
    std::thread t1([&stack] {stack.push(1);});
    std::thread t2([&stack] {stack.push(2);});
    std::thread t3([&stack] {stack.push(3);});

    std::thread t4([&stack] {
        std::stringstream ss;
        auto p = stack.pop().get();
        ss << (p ? *p : 0) << std::endl;
        std::cout << ss.str();
    });
    std::thread t5([&stack] {
        std::stringstream ss;
        auto p = stack.pop().get();
        ss << (p ? *p : 0) << std::endl;
        std::cout << ss.str();
    });
    std::thread t6([&stack] {
        std::stringstream ss;
        auto p = stack.pop().get();
        ss << (p ? *p : 0) << std::endl;
        std::cout << ss.str();
    });
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();
    return 0;
}
