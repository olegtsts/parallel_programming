#include <iostream>
#include <atomic>
#include <memory>

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
        new_node.ptr->next = head.load();
        while (!head.compare_exchange_weak(new_node.ptr->next, new_node)) {}
    }
    std::shared_ptr<T> pop() {
        CountedNodePtr old_head = head.load();
        while (true) {
            increase_head_count(old_head);
            Node* ptr = old_head.ptr;
            if (!ptr) {
                return std::shared_ptr<T>();
            }
            if (head.compare_exchange_strong(old_head, ptr->next)) {
                std::shared_ptr<T> res;
                res.swap(ptr->data);
                int count_increase = old_head.external_count - 2;
                if (ptr->internal_count.fetch_add(count_increase) == -count_increase) {
                    delete ptr;
                }
                return res;
            } else if (ptr->internal_count.fetch_sub(1) == 1) {
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
        int external_count;
        Node* ptr;
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
        } while (!head.compare_exchange_strong(old_counter, new_counter));
        old_counter.external_count = new_counter.external_count;
    }
    std::atomic<CountedNodePtr> head;
};
int main() {
    LockFreeStack<int> stack;
    stack.push(5);
    std::cout << *stack.pop() << std::endl;
    return 0;
}
