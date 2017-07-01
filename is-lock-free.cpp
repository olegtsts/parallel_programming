#include <iostream>
#include <memory>
#include <atomic>

int main() {
    std::shared_ptr<int> a;
    std::cout << (std::atomic_is_lock_free(&a) ? 1 : 0) << "\n";
    return 0;
}
