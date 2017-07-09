#include <iostream>
#include <queue>
#include <future>
#include <mutex>
#include <condition_variable>
#include <cassert>

class Queue {
public:
    Queue(size_t max_size)
    : max_size(max_size)
    {}

    void Push(const int element){
        std::unique_lock<std::mutex> lock(m);

        full_condition.wait(lock, [this] {return queue.size() < max_size;});
        queue.push(element);
        std::cout << "Pushed " << element << std::endl;
        empty_condition.notify_all();
    }
    int Pop() {
        std::unique_lock<std::mutex> lock(m);
        empty_condition.wait(lock, [this] {return queue.size() > 0;});
        int element = queue.front();
        queue.pop();
        std::cout << "Popped " << element << std::endl;
        full_condition.notify_all();
        return element;
    }

    bool Empty() {
        std::unique_lock<std::mutex> lk(m);
        return queue.empty();
    }
private:
    size_t max_size;
    std::condition_variable full_condition;
    std::condition_variable empty_condition;
    std::mutex m;
    std::queue<int> queue;
};

void TestConcurrentPushAndPopOnEmptyQueue() {
    Queue q(2);
    std::promise<void> go, push_ready, pop_ready;
    std::shared_future<void> ready_to_go(go.get_future());

    std::future<void> push_done;
    std::future<int> pop_done;

    try {
        push_done = std::async(std::launch::async,
                               [&q, ready_to_go, &push_ready] () {
                                   push_ready.set_value();
                                   ready_to_go.wait();
                                   q.Push(42);
                                });
        pop_done = std::async(std::launch::async,
                              [&q, ready_to_go, &pop_ready] () {
                                  pop_ready.set_value();
                                  ready_to_go.wait();
                                  return q.Pop();
                                });
        push_ready.get_future().wait();
        pop_ready.get_future().wait();
        go.set_value();
        push_done.get();
        assert(pop_done.get() == 42);
        assert(q.Empty());
    } catch (...) {
        go.set_value();
        throw;
    }
}
int main() {
    TestConcurrentPushAndPopOnEmptyQueue();
    return 0;
}
