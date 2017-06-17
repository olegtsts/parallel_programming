#include <iostream>
#include <memory>

template <typename T>
class Queue {
private:
    struct Node {
        T data;
        std::unique_ptr<Node> next = nullptr;

        Node(T data_)
        : data(std::move(data_))
        {}
    };
public:
    Queue() {}

    Queue& operator=(const Queue& other) = delete;
    Queue(const Queue& other) = delete;

    std::shared_ptr<T> try_pop() {
        if (head == nullptr) {
            return {};
        }
        std::shared_ptr<T> res(std::make_shared<T>(std::move(head->data)));
        std::unique_ptr<Node> old_head(std::move(head));
        head = std::move(old_head->next);
        return res;
    }
    void push(T new_value) {
        std::unique_ptr<Node> p(new Node(std::move(new_value)));
        if (tail) {
            tail->next = std::move(p);
        } else {
            Node* new_tail = p.get();
            head = std::move(p);
            tail = new_tail;
        }
    }
private:
    std::unique_ptr<Node> head;
    Node * tail = nullptr;
};
int main() {
    Queue<int> queue;
    queue.push(4);
    std::cout << *queue.try_pop() << std::endl;
    return 0;
}
