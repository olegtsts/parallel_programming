#include <iostream>
#include <memory>
#include <mutex>

template <typename T>
class Queue {
private:
    struct Node {
        std::shared_ptr<T> value;
        std::unique_ptr<Node> next;
    };

    Node* get_tail() {
        std::lock_guard<std::mutex> lock(tail_mutex);
        return tail;
    }
    std::unique_ptr<Node> pop_head() {
        std::lock_guard<std::mutex> lock(head_mutex);
        if (head.get() == get_tail()) {
            return nullptr;
        }
        std::unique_ptr<Node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }
public:
    Queue()
    : head(new Node())
    , tail(head.get())
    {}
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    std::shared_ptr<T> try_pop() {
        std::unique_ptr<Node> head = pop_head();
        return head ? head->value : std::shared_ptr<T>();
    }
    void push(T new_value) {
        std::shared_ptr<T> data(std::make_shared<T>(std::move(new_value)));
        std::unique_ptr<Node> new_node(new Node());
        std::lock_guard<std::mutex> lock(tail_mutex);
        Node* new_tail = new_node.get();
        tail->next = std::move(new_node);
        tail->value = data;
        tail = new_tail;
    }
private:
    std::mutex head_mutex;
    std::unique_ptr<Node> head;
    Node* tail;
    std::mutex tail_mutex;
};
int main() {
    Queue<int> queue;
    queue.push(5);
    std::cout << *queue.try_pop() << std::endl;
    return 0;
}
