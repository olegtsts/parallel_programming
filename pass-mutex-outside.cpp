#include <iostream>
#include <thread>
#include <mutex>

std::mutex mu;

std::unique_lock<std::mutex> do_work() {
    std::unique_lock<std::mutex> lock(mu);
    std::cout << "Start here\n";
    return lock;
}

void do_another_work() {
    std::unique_lock<std::mutex> lock = do_work();
    std::cout << lock.owns_lock() << " Finish here\n";
}

int main() {
    std::thread t1(do_another_work);
    std::thread t2(do_another_work);
    t1.join();
    t2.join();
    return 0;
}
