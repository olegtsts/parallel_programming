#include <mutex>
#include <condition_variable>
#include <memory>
#include <queue>
#include <iostream>

template <typename T>
class SharedPtrQueue {
public:
    SharedPtrQueue()
    {}
    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [this] {return !data_queue.empty();});
        value = std::move(*data_queue.front());
        data_queue.pop();
    }
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mu);
        if (data_queue.empty()) {
            return false;
        }
        value = std::move(*data_queue.front());
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [this] {return !data_queue.empty();});
        std::shared_ptr<T> ptr = data_queue.front();
        data_queue.pop();
        return ptr;
    }
    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lock(mu);
        if (data_queue.empty()) {
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> res = data_queue.top();
        data_queue.pop();
        return res;
    }
    void push(T new_value) {
        std::shared_ptr<T> cell(std::make_shared<T>(new_value));
        std::lock_guard<std::mutex> lock(mu);
        data_queue.push(cell);
        cv.notify_one();
    }
    bool empty() const {
        std::lock_guard<std::mutex> lock(mu);
        return data_queue.empty();
    }
private:
    mutable std::mutex mu;
    std::queue<std::shared_ptr<T> > data_queue;
    std::condition_variable cv;
};
int main() {
    SharedPtrQueue<int> queue;
    queue.push(4);
    std::cout << *queue.wait_and_pop() << std::endl;
    return 0;
}
