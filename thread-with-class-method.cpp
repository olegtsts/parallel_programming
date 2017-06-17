#include <iostream>
#include <thread>

class Worker {
public:
    void work() const {
        std::cout << "I am working\n";
    }
};

int main() {
    Worker worker;
    std::thread(&Worker::work, &worker).join();
    return 0;
}

