#include <thread>
#include <iostream>

class Callable {
public:
    void operator() () {
        std::cout << "Hello\n";
    }
};

int main () {
    std::thread t{Callable()};
    t.join();
    std::thread([] {
            std::cout << "Hello2\n";
    }).detach();
    return 0;
}
