#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <vector>
#include <future>
#include <functional>

class Queue {
public:
    Queue(size_t max_size)
    : max_size(max_size)
    {}

    void Push(const int element){
        std::unique_lock<std::mutex> lock(m);

        full_condition.wait(lock, [this] {return queue.size() < max_size;});
        queue.push(element);
//        std::cout << "Pushed " << element << std::endl;
        empty_condition.notify_all();
    }
    int Pop() {
        std::unique_lock<std::mutex> lock(m);
        empty_condition.wait(lock, [this] {return queue.size() > 0;});
        int element = queue.front();
        queue.pop();
//        std::cout << "Popped " << element << std::endl;
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

void thread_push(const int number, Queue& queue, std::shared_future<void> wait_to_go) noexcept {
    wait_to_go.wait();
    for (size_t i = 0;i < 10000000; ++i) {
        queue.Push(number);
    }
}
void thread_pop(Queue& queue, std::shared_future<void> wait_to_go) noexcept {
    wait_to_go.wait();
    for (size_t i = 0; i < 10000000; ++i) {
        queue.Pop();
//        std::stringstream ss;
//        ss <<  (ptr.get() ? *ptr : 0) << std::endl;
//        std::cerr << ss.str();
    }
}

int main () {
    Queue queue(10000);
    std::promise<void> go;
    std::shared_future<void> wait_to_go = go.get_future();
    std::vector<std::thread> threads;
    for (size_t i = 0; i < 1; ++i) {
        threads.push_back(std::thread(thread_push, i + 100, std::ref(queue), wait_to_go));
    }
    for (size_t i = 0; i < 1; ++i) {
        threads.push_back(std::thread(thread_pop, std::ref(queue), wait_to_go));
    }
    go.set_value();
    for (auto& one_thread: threads) {
        one_thread.join();
    }
//    std::thread first_thread([&]() {
//        queue.push(1);
//        queue.push(2);
//        queue.push(3);
//        queue.push(4);
//    });
//
//    std::thread second_thread([&]() {
//        queue.pop();
//        queue.pop();
//        queue.pop();
//        queue.pop();
//        queue.pop();
//    });
//
//    first_thread.join();
//    second_thread.join();

    return 0;
}
