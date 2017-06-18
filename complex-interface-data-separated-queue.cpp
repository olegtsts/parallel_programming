#include <iostream>
#include <mutex>
#include <condition_variable>

template <typename T>
class Queue {
private:
    struct Node {
        std::shared_ptr<T> data;
        std::unique_ptr<Node> next;
    };
    Node* get_tail() {
        std::lock_guard<std::mutex> lock(tail_mutex);
        return tail;
    }
    std::unique_ptr<Node> pop_head() {
        std::unique_ptr<Node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }
    std::unique_lock<std::mutex> wait_for_data() {
        std::unique_lock<std::mutex> lock(head_mutex);
        cond.wait(head_mutex, [this] {return head.get() != get_tail();});
        return std::move(lock);
    }
    std::unique_ptr<Node> wait_pop_head() {
        std::unique_lock<std::mutex> lock(wait_for_data());
        return pop_head();
    }
    std::unique_ptr<Node> wait_pop_head(T& value) {
        std::unique_lock<std::mutex> lock(wait_for_data());
        value = std::move(*head->data);
        return pop_head();
    }
    std::unique_ptr<Node> try_pop_head() {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail()) {
            return {};
        }
        return pop_head();
    }
    std::unique_ptr<Node> try_pop_head(T& value) {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail()) {
            return {};
        }
        value = std::move(*head->data);
        return pop_head();
    }
public:
    Queue()
    : head(new Node())
    , tail(head.get())
    {}

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    std::shared_ptr<T> wait_and_pop() {
        std::unique_ptr<Node> old_head = wait_pop_head();
        return old_head->data;
    }

    void wait_and_pop(T& value) {
        wait_pop_head(value);
    }

    std::shared_ptr<T> try_pop() {
        std::unique_ptr<Node> old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }

    bool try_pop(T& value) {
        return try_pop_head(value);
    }

    void push(T new_value) {
        std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
        std::unique_ptr<Node> p(new Node());
        {
            std::lock_guard<std::mutex> lock(tail_mutex);
            tail->data = new_data;
            Node* new_tail = p.get();
            tail->next = std::move(p);
            tail = new_tail;
        }
        cond.notify_one();
    }

private:
    std::mutex head_mutex;
    std::mutex tail_mutex;
    std::unique_ptr<Node> head;
    Node* tail;
    std::condition_variable cond;
};

int main () {
    Queue<int> queue;
    queue.push(5);
    std::cout << *queue.try_pop() << std::endl;
    return 0;
}
