#include <iostream>
#include <thread>

void do_work() {
    std::cout << "Hello\n";
}

void join_thread(std::thread t) {
    t.join();
}

int main() {
    std::thread t(do_work);
    join_thread(std::move(t));
}
