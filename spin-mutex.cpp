#include <atomic>

class SpinLockMutex {
public:
    SpinLockMutex()
    : flag(ATOMIC_FLAG_INIT)
    {}
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {}
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }
private:
    std::atomic_flag flag;
};
int main() {
    SpinLockMutex lock;
    lock.lock();
    lock.unlock();
    return 0;
}
