#include <iostream>
#include <future>
#include <cstdio>

int some_operation() {
    printf("Doing operation\n");
    return 42;
}
int main() {
    std::shared_future<int> f = std::async(some_operation);
    std::thread t1([&] {
        std::shared_future<int> f1(f);
        printf("Result: %d\n", f1.get());
    });
    std::thread t2([&] {
        std::shared_future<int> f2(f);
        printf("Result: %d\n", f2.get());
    });
    t1.join();
    t2.join();
    return 0;
}
