#include <iostream>
#include <thread>

void do_work(std::unique_ptr<int> ptr) {
    std::cout << *ptr << std::endl;
}

int main() {
    std::unique_ptr<int> ptr(new int(5));
    std::thread(do_work, std::move(ptr)).join();
    return 0;
}

