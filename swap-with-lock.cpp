#include <iostream>
#include <mutex>
#include <thread>
#include <algorithm>
#include <cstdio>

struct Simple {
    Simple(const int content)
    : content(content)
    {}
    friend void swap(Simple& first, Simple& second) {
        if (&first == &second) {
            return;
        }
        std::lock(first.mu, second.mu);
        std::lock_guard<std::mutex> first_lock(first.mu, std::adopt_lock);
        std::lock_guard<std::mutex> second_lock(second.mu, std::adopt_lock);
        std::swap(first.content, second.content);
    }

int content;
private:
    std::mutex mu;
};

int main() {
    Simple a(1);
    Simple b(2);
    std::thread t1([&] {
        swap(a, b);
    });
    std::thread t2([&] {
        printf("%d %d\n", a.content, b.content);
    });
    t1.join();
    t2.join();
    return 0;
}
