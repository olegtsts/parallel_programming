#include <atomic>
#include <thread>
#include <cassert>
#include <iostream>

std::atomic<bool> x,y;
std::atomic<int> z;

void write_x() {
    x.store(true, std::memory_order_release);
}
void write_y() {
    y.store(true, std::memory_order_release);
}
void read_x_then_y() {
    while (!x.load(std::memory_order_acquire)) {}
    if (y.load(std::memory_order_acquire)) {
        ++z;
    }
}
void read_y_then_x() {
    while (!y.load(std::memory_order_acquire)) {}
    if (x.load(std::memory_order_acquire)) {
        ++z;
    }
}
int main() {
    x = false;
    y = false;
    z = 0;
    std::thread t1(write_x);
    std::thread t2(write_y);
    std::thread t3(read_x_then_y);
    std::thread t4(read_y_then_x);
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    std::cout << z.load() << std::endl;
    //Assertion my fail
    assert(z.load() != 0);
    return 0;
}
