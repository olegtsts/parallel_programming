#include <future>
#include <iostream>
#include <thread>

int some_operation() {
    return 42;
}

int main() {
    std::future<int> result = std::async(some_operation);
    std::cout << "Result = " << result.get() << std::endl;
    return 0;
}
