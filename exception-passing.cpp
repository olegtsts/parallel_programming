#include <iostream>
#include <future>
#include <thread>

int main() {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    std::thread t1([&] {
        try {
            throw std::logic_error("logic error");
        } catch(...) {
            promise.set_exception(std::current_exception());
        }
    });
    std::thread t2([&] {
        future.get();
    });
    t1.join();
    t2.join();
    return 0;
}
