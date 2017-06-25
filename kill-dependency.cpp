#include <iostream>
#include <atomic>

std::atomic<int> var;
int main() {
    int value = std::kill_dependency(var.load(std::memory_order_consume));
    return 0;
}
