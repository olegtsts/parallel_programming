#include <iostream>
#include <thread>


class ThreadGuard {
public:
    ThreadGuard(std::thread& thread)
    : thread(thread)
    {}

    ~ThreadGuard() {
        if (thread.joinable()) {
            thread.join();
        }
    }

    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;
private:
    std::thread& thread;
};

void hello() {
    std::cout << "Hello\n";
}

int main() {
    std::thread t(hello);
    ThreadGuard{t};
    return 0;
}
