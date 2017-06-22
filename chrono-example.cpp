#include <iostream>
#include <chrono>
#include <cstdio>
#include <future>

int do_something() {
    return 42;
}

int main() {
    std::chrono::seconds g(100);
    std::chrono::duration<int, std::ratio<60, 1> > g2 = std::chrono::duration_cast<std::chrono::duration<int, std::ratio<60, 1> > >(g);
    std::cout << g2.count() << std::endl;
    std::chrono::milliseconds ms(45678);
    std::chrono::seconds s = std::chrono::duration_cast<std::chrono::seconds>(ms);
    std::future<int> f = std::async(std::launch::async, do_something);
    if (f.wait_for(std::chrono::milliseconds(35)) == std::future_status::ready) {
        printf("%d\n", f.get());
    }
}
