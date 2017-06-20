#include <iostream>
#include <thread>
#include <mutex>
#include <climits>
#include <stdexcept>
#include <cstdio>

class HeirarchicalMutex {
public:
    HeirarchicalMutex(const size_t level)
    : prev_hierarchical_level(0)
    , this_mutex_hierarchical_level(level)
    {}

    void lock() {
        check_level();
        mu.lock();
        update_hierarchical_level();
    }
    void unlock() {
        current_thread_hierarchical_level = prev_hierarchical_level;
        mu.unlock();
    }
    bool  try_lock() {
        check_level();
        bool is_locked = mu.try_lock();
        if (is_locked) {
            update_hierarchical_level();
        }
        return is_locked;
    }
private:
    void check_level() {
        if (current_thread_hierarchical_level < this_mutex_hierarchical_level) {
            std::string str(50, ' ');
            sprintf(&str[0], "Hierarchy violation: %lu > %lu",
                        this_mutex_hierarchical_level, current_thread_hierarchical_level);
            throw std::logic_error(str);
        }
    }
    void update_hierarchical_level() {
        prev_hierarchical_level = current_thread_hierarchical_level;
        current_thread_hierarchical_level = this_mutex_hierarchical_level;
    }

    std::mutex mu;
    size_t prev_hierarchical_level;
    size_t this_mutex_hierarchical_level;
    static thread_local size_t current_thread_hierarchical_level;
};

thread_local size_t HeirarchicalMutex::current_thread_hierarchical_level = ULLONG_MAX;

int main() {
    HeirarchicalMutex mu1(1);
    HeirarchicalMutex mu2(10);
    std::thread t1([&] {
         std::lock_guard<HeirarchicalMutex> lock(mu1);
         std::lock_guard<HeirarchicalMutex> lock2(mu2);
    });
    t1.join();
    return 0;
}
