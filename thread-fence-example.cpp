#include <iostream>
#include <atomic>
#include <thread>
#include <cassert>

std::atomic<bool> x,y;
std::atomic<int> z;

void write_x_then_y() {
    x.store(true, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    y.store(true, std::memory_order_relaxed);
}
void read_y_then_x() {
    while (!y.load(std::memory_order_relaxed)) {}
    std::atomic_thread_fence(std::memory_order_acquire);
    if (x.load(std::memory_order_relaxed)) {
        ++z;
    }
}
int main() {
    x = false;
    y = false;
    z = 0;
    std::thread t1(write_x_then_y);
    std::thread t2(read_y_then_x);
    t1.join();
    t2.join();
    assert(z.load() != 0);
    return 0;
}
