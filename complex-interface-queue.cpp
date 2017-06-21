#include <cstdio>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <memory>

template <typename T>
class ComplexInterfaceQueue {
public:
    ComplexInterfaceQueue() {}
    ComplexInterfaceQueue(const ComplexInterfaceQueue& other) {
        std::lock_guard<std::mutex> lock(mu);
        data = other.data;
    }
    void push(T new_value) {
        std::lock_guard<std::mutex> lock(mu);
        data.push(new_value);
        cond.notify_one();
    }
    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mu);
        cond.wait(lock, [this] {return !data.empty();});
        value = data.front();
        data.pop();
    }
    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lock(mu);
        cond.wait(lock, [this] {return !data.empty();});
        std::shared_ptr<T> ptr(std::make_shared<T>(data.front()));
        data.pop();
        return ptr;
    }
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mu);
        if (data.empty()) {
            return false;
        }
        value = data.front();
        data.pop();
        return true;
    }
    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lock(mu);
        if (data.empty()) {
            return std::shared_ptr<T>();
        }
        std::shared_ptr<T> ptr(std::make_shared<T>(data.front()));
        data.pop();
        return ptr;
    }
    bool empty() const {
        std::lock_guard<std::mutex> lock(mu);
        return data.empty();
    }
private:
    mutable std::mutex mu;
    std::queue <T> data;
    std::condition_variable cond;
};
int main () {
    ComplexInterfaceQueue<int> queue;
    std::thread t1([&] {
        queue.push(1);
    });
    std::thread t2([&] {
        printf("%d\n", *queue.wait_and_pop());
    });
    t1.join();
    t2.join();
    return 0;
}
