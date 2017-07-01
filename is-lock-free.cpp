#include <iostream>
#include <memory>
#include <atomic>

int main() {
    std::shared_ptr<int> a;
    std::cout << std::boolalpha << std::atomic_is_lock_free(&a) << "\n";
    return 0;
}
