#include <iostream>
#include <thread>

class ScopedThread {
public:
    ScopedThread(std::thread t_)
    : t(std::move(t_))
    {
        if (!t.joinable()) {
            throw std::logic_error("No thread");
        }
    }
    ~ScopedThread() {
        t.join();
    }
    ScopedThread(const ScopedThread&) = delete;
    ScopedThread& operator=(const ScopedThread&) = delete;
private:
    std::thread t;
};

void do_work() {
    std::cout << "Hello!\n";
}

int main() {
    ScopedThread{std::thread(do_work)};
    return 0;
}
