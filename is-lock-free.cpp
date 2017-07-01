#include <iostream>
#include <memory>
#include <atomic>
#include <thread>


struct __attribute__ ((packed))  Small {
    int * b;
    int a;
};

int main() {
    std::atomic<std::thread::id> b;
    std::atomic<void*> c;
    std::shared_ptr<int> a;
    std::atomic<Small> small;
    std::cout << std::boolalpha << std::atomic_is_lock_free(&a) << " " << small.is_lock_free() << " "
        << std::atomic_is_lock_free(&b) << " " << std::atomic_is_lock_free(&c) << " " << sizeof(int*) << "\n";
    return 0;
}
