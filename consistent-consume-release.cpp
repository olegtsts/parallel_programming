#include <iostream>
#include <atomic>
#include <string>
#include <chrono>
#include <thread>
#include <cassert>

struct X {
    int i;
    std::string s;
};

std::atomic<X*> p;
std::atomic<int> a;

void set_atomic_pointer() {
    X* x = new X();
    x->i = 42;
    x->s = "hello";
    a.store(1, std::memory_order_relaxed);
    p.store(x, std::memory_order_release);
}
void use_atomic_pointer() {
    X* x;
    while (!(x = p.load(std::memory_order_consume))) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    //This never fails
    assert(x->i == 42);
    assert(x->s == "hello");
    //This may fail
    assert(a.load(std::memory_order_relaxed) == 1);
}
int main() {
    std::thread t1(set_atomic_pointer);
    std::thread t2(use_atomic_pointer);
    t1.join();
    t2.join();
    return 0;
}
