#include <future>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>
#include <vector>

class ThreadInterrupted {};

void InterruptionPoint();
class InterruptibleFlag {
public:
    InterruptibleFlag()
    : thread_cond(nullptr)
    , thread_cond_any(nullptr)
    {}

    void Set() {
        flag.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        if (thread_cond) {
            thread_cond->notify_all();
        } else if (thread_cond_any) {
            thread_cond_any->notify_all();
        }
    }
    bool IsSet() const {
        return flag.load(std::memory_order_relaxed);
    }

    void SetConditionVariable(std::condition_variable& cv) {
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        thread_cond = &cv;
    }
    void ClearConditionVariable() {
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        thread_cond = nullptr;
    }


    template <typename Lockable>
    void Wait(std::condition_variable_any& cv, Lockable& lk) {
        struct CustomLock {
            CustomLock(InterruptibleFlag* self, std::condition_variable_any& cond,
                    Lockable& inner_lock)
            : self(self)
            , inner_lock(inner_lock) {
                self->set_clear_mutex.lock();
                self->thread_cond_any = &cond;
            }

            void unlock() {
                inner_lock.unlock();
                self->set_clear_mutex.unlock();
            }

            void lock() {
                std::lock(self->set_clear_mutex, inner_lock);
            }

            ~CustomLock() {
                self->thread_cond_any = nullptr;
                self->set_clear_mutex.unlock();
            }
            InterruptibleFlag* self;
            Lockable& inner_lock;
        };
        CustomLock cl(this, cv, lk);
        InterruptionPoint();
        cv.wait(cl);
        InterruptionPoint();
    }
private:
    std::atomic<bool> flag;
    std::condition_variable* thread_cond;
    std::mutex set_clear_mutex;
    std::condition_variable_any* thread_cond_any;
};

thread_local InterruptibleFlag this_thread_interrupt_flag;

struct ClearCvOnDestruct {
    ~ClearCvOnDestruct() {
        this_thread_interrupt_flag.ClearConditionVariable();
    }
};

class InterruptibleThread {
public:
    template <typename TFunction>
    InterruptibleThread(TFunction f) {
        std::promise<InterruptibleFlag*> p;
        internal_thread = std::thread([f, &p] {
            p.set_value(&this_thread_interrupt_flag);
            try {
                f();
            } catch (const ThreadInterrupted& ) {
            }
        });
        flag = p.get_future().get();
    }
    void Join() {
        internal_thread.join();
    }
    void Interrupt() {
        if (flag) {
            flag->Set();
        }
    }
private:
    std::thread internal_thread;
    InterruptibleFlag* flag;
};

void InterruptionPoint() {
    if (this_thread_interrupt_flag.IsSet()) {
        throw ThreadInterrupted();
    }
}
void InterruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk) {
    InterruptionPoint();
    this_thread_interrupt_flag.SetConditionVariable(cv);
    ClearCvOnDestruct guard;
    InterruptionPoint();
    cv.wait_for(lk, std::chrono::milliseconds(1));
    InterruptionPoint();
}
template <typename Predicate>
void InterruptibleWait(std::condition_variable& cv, std::unique_lock<std::mutex>& lk,
        Predicate pred) {
    InterruptionPoint();
    this_thread_interrupt_flag.SetConditionVariable(cv);
    ClearCvOnDestruct guard;
    while (!this_thread_interrupt_flag.IsSet() && !pred()) {
        cv.wait_for(lk, std::chrono::milliseconds(1));
    }
    InterruptionPoint();
}
template <typename Lockable>
void InterruptibleWait(std::condition_variable_any& cv, Lockable& lk) {
    this_thread_interrupt_flag.Wait(cv, lk);
}
template <typename T>
void InterruptibleWait(std::future<T>& uf) {
    while (!this_thread_interrupt_flag.IsSet()) {
        if (uf.wait_for(std::chrono::milliseconds(1)) == std::future_status::ready) {
            break;
        }
    }
    InterruptionPoint();
}

std::vector<InterruptibleThread> background_threads;

void BackgroundThread(int disk_id) {
    while (true) {
        InterruptionPoint();
        std::stringstream ss;
        ss << "Start updating index " << disk_id << "\n";
        std::cout << ss.str();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ss.str("");
        ss << "Stop updating index " << disk_id << "\n";
        std::cout << ss.str();
    }
}
void StartBackgroundProcessing() {
    background_threads.push_back(InterruptibleThread(std::bind(BackgroundThread, 1)));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    background_threads.push_back(InterruptibleThread(std::bind(BackgroundThread, 2)));
}
int main() {
    StartBackgroundProcessing();
    std::cin.get();
    for (auto& one_thread: background_threads) {
        one_thread.Interrupt();
    }
    for (auto& one_thread: background_threads) {
        one_thread.Join();
    }
    return 0;
}
