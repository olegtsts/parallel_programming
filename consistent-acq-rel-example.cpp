#include <iostream>
#include <atomic>
#include <thread>
#include <cassert>

std::atomic<int> data[2];
std::atomic<bool> sync1(false), sync2(false);

void first_thread() {
    data[0].store(1, std::memory_order_release);
    data[1].store(2, std::memory_order_release);
    sync1.store(true, std::memory_order_release);
}
void second_thread() {
    while (!sync1.load(std::memory_order_acquire)) {}
    sync2.store(std::memory_order_release);
}
void third_thread() {
    while (!sync2.load(std::memory_order_acquire)){}
    assert(data[0].load() == 1);
    assert(data[1].load() == 2);
}
int main() {
    std::thread t1(first_thread);
    std::thread t2(second_thread);
    std::thread t3(third_thread);
    t1.join();
    t2.join();
    t3.join();
    return 0;
}
