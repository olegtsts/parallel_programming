#include <atomic>
#include <iostream>
#include <thread>

class Barrier {
public:
    explicit Barrier(const int waiting_count)
    : waiting_count(waiting_count)
    , spaces(waiting_count)
    , generation(0)
    {}

    void Wait() {
        const int my_generation = generation;
        if (!--spaces) {
            spaces = waiting_count.load();
            ++generation;
        } else {
            while (generation == my_generation) {
                std::this_thread::yield();
            }
        }
    }
    void DoneWaiting() {
        --waiting_count;
        if (!--spaces) {
            spaces = waiting_count.load();
            ++generation;
        }
    }
private:
    std::atomic<int> waiting_count;
    std::atomic<int> spaces;
    std::atomic<int> generation;
};
int main() {
    Barrier bar(2);
    std::thread t1([&] {
        bar.Wait();
        std::cout << "Thread 1\n";
    });
    std::thread t2([&] {
        bar.Wait();
        std::cout << "Thread 2\n";
    });
    std::thread t3([&] {
        bar.Wait();
        std::cout << "Thread 3\n";
    });
    std::thread t4([&] {
        bar.Wait();
        std::cout << "Thread 4\n";
    });
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    return 0;
}
