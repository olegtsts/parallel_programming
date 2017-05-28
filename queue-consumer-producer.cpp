#include <iostream>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <queue>

class Queue {
public:
    Queue(size_t max_size)
    : max_size(max_size)
    {}

    void push(const int element){
        std::unique_lock<std::mutex> lock(m);

        while (queue.size() >= max_size) {
            full_condition.wait(lock);
        }
        queue.push(element);
        std::cout << "Pushed " << element << std::endl;
        empty_condition.notify_all();
    }
    int pop() {
        std::unique_lock<std::mutex> lock(m);
        while (queue.size() == 0) {
            empty_condition.wait(lock);
        }
        int element = queue.front();
        queue.pop();
        std::cout << "Popped " << element << std::endl;
        full_condition.notify_all();
        return element;
    }

private:
    size_t max_size;
    std::condition_variable full_condition;
    std::condition_variable empty_condition;
    std::mutex m;
    std::queue<int> queue;
};


int main () {
    Queue queue(2);
    std::thread first_thread([&]() {
        queue.push(1);
        queue.push(2);
        queue.push(3);
        queue.push(4);
    });

    std::thread second_thread([&]() {
        queue.pop();
        queue.pop();
        queue.pop();
        queue.pop();
        queue.pop();
    });

    first_thread.join();
    second_thread.join();

    return 0;
}
