#include <iostream>
#include <thread>
#include <mutex>
#include <climits>
#include <stdexcept>
#include <cstdio>
#include <set>

class HeirarchicalMutex {
public:
    HeirarchicalMutex(const size_t level)
    : this_mutex_hierarchical_level(level)
    {}

    void lock() {
        check_level();
        mu.lock();
        active_levels.insert(this_mutex_hierarchical_level);
    }
    void unlock() {
        active_levels.erase(this_mutex_hierarchical_level);
        mu.unlock();
    }
    bool  try_lock() {
        check_level();
        bool is_locked = mu.try_lock();
        if (is_locked) {
            active_levels.insert(this_mutex_hierarchical_level);
        }
        return is_locked;
    }
private:
    void check_level() {
        if (active_levels.size() > 0 && *active_levels.begin() < this_mutex_hierarchical_level) {
            std::string str(50, ' ');
            sprintf(&str[0], "Hierarchy violation: %lu > %lu",
                        this_mutex_hierarchical_level, *active_levels.begin());
            throw std::logic_error(str);
        }
    }
    std::mutex mu;
    size_t this_mutex_hierarchical_level;
    static thread_local std::set<size_t> active_levels;
};

thread_local std::set<size_t> HeirarchicalMutex::active_levels;

int main() {
    HeirarchicalMutex mu1(1);
    HeirarchicalMutex mu2(2);
    HeirarchicalMutex mu3(3);
    std::thread t1([&] {
        mu3.lock();
        mu1.lock();
        mu3.unlock();
        mu2.lock();
    });
    t1.join();
    return 0;
}
