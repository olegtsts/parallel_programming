#include <thread>
#include <iostream>

class Callable {
public:
    void operator() () {
        std::cout << "Hello\n";
    }
};

int main () {
    Callable callable;
    std::thread t(callable);
    t.join();
    return 0;
}
