#include <iostream>
#include <memory>

template<typename T>
class Queue {
private:
    struct Node {
        std::unique_ptr<Node> next;
        std::shared_ptr<T> value;
    };
public:
    Queue()
    : head(new Node())
    , tail(head.get())
    {}

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    std::shared_ptr<T> try_pop() {
        if (head.get() == tail) {
            return {};
        }
        std::shared_ptr<T> new_value(head->value);
        std::unique_ptr<Node> old_head = std::move(head);
        head = std::move(old_head->next);
        return new_value;
    }
    void push(T new_value) {
        std::shared_ptr<T> pointer(std::make_shared<T>(std::move(new_value)));
        std::unique_ptr<Node> new_node(new Node());
        tail->value = pointer;
        Node* new_tail = new_node.get();
        tail->next = std::move(new_node);
        tail = new_tail;
    }
private:
        std::unique_ptr<Node> head;
        Node* tail;
};
int main() {
    Queue<int> queue;
    queue.push(5);
    std::cout << *queue.try_pop() << std::endl;
    return 0;
}
