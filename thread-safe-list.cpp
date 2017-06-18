#include <iostream>
#include <mutex>
#include <shared_mutex>

template <typename T>
class ThreadSafeList {
public:
    ThreadSafeList() {}
    ~ThreadSafeList() {
        remove_if([](const Node&) {return true;});
    }
    ThreadSafeList(const ThreadSafeList&) = delete;
    ThreadSafeList& operator=(const ThreadSafeList&) = delete;

    void push_front(const T& value) {
        std::unique_ptr<Node> new_node(new Node(value));
        std::lock_guard<std::mutex> lock(head.mutex);
        new_node->next = std::move(head.next);
        head.next = std::move(new_node);
    }

    template <typename Function>
    void for_each(Function f) {
        Node* current = &head;
        std::unique_lock<std::mutex> lock(head.mutex);
        while (Node* next = current->next.get()) {
            std::unique_lock<std::mutex> next_lock(next->mutex);
            lock.unlock();
            f(*next->data);
            current = next;
            lock = std::move(next_lock);
        }
    }

    template <typename Predicate>
    std::shared_ptr<T> find_first_if(Predicate d) {
        Node* current = &head;
        std::unique_lock<std::mutex> lock(head.mutex);
        while (Node* next = current->next.get()) {
            std::unique_lock<std::mutex> next_lock(next->mutex);
            lock.unlock();
            if (p(*next->data)) {
                return next->data;
            }
            current = next;
            lock = std::move(next_lock);
        }
        return std::shared_ptr<T>();
    }

    template <typename Predicate>
    void remove_if(Predicate p) {
        Node* current = &head;
        std::unique_lock<std::mutex> lock(head.mutex);
        while (Node* next = current->next.get()) {
            std::unique_lock<std::mutex> next_lock(next->mutex);
            if (p(*next->data)) {
                std::unique_ptr<Node> old_next = std::move(current->next);
                current->next = std::move(next->next);
                next_lock.unlock();
            } else {
                lock.unlock();
                current = next;
                lock = std::move(next_lock);
            }
        }
    }
private:
    struct Node {
        std::mutex mutex;
        std::shared_ptr<T> data;
        std::unique_ptr<Node> next;

        Node()
        {}

        Node(const T& value)
        : data(std::make_shared<T>(value))
        {}
    };

    Node head;
};
int main() {
    ThreadSafeList<int> list;
    list.push_front(3);
    list.push_front(4);
    list.for_each([&](const int element) {
        std::cout << element << std::endl;
    });
    return 0;
}
