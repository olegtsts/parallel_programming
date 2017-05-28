#include <iostream>
#include <thread>
#include <atomic>

std::atomic<int> number(100);
std::atomic<int> number2(200);

thread_local unsigned int var = 1;

void first_thread_func() {
    number.store(111, std::memory_order_release);
    number2.store(222, std::memory_order_release);
    var += 1;
    std::cout << var << std::endl;
}


void second_thread_func() {
    while (number2.load(std::memory_order_acquire) != 222) {sched_yield();}
    std::cout <<  number.load(std::memory_order_relaxed) << " " << number2.load(std::memory_order_relaxed) << std::endl;
    var += 1;
    std::cout << var << std::endl;
}

int main() {
    std::thread first_thread(first_thread_func);
    std::thread second_thread(second_thread_func);

    first_thread.join();
    second_thread.join();
    return 0;
}
