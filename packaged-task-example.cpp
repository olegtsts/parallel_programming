#include <iostream>
#include <future>
#include <mutex>
#include <thread>
#include <deque>

std::mutex mu;
std::deque<std::packaged_task<void() > > tasks;
void process_tasks() {
    int processed_tasks = 0;
    while (true) {
        std::packaged_task<void()> task;
        {
            std::lock_guard<std::mutex> lock(mu);
            if (tasks.empty()) {
                continue;
            }
            task = std::move(tasks.front());
            tasks.pop_front();
        }
        task();
        ++processed_tasks;
        if (processed_tasks >= 2) {
            break;
        }
    }
}
std::future<void> post_task() {
    std::packaged_task<void()> task([] {std::cout << "Hello\n";});
    std::future<void> res = task.get_future();
    std::lock_guard<std::mutex> lock(mu);
    tasks.push_back(std::move(task));
    return res;
}
int main() {
    std::thread t1([] {
        auto f1 = post_task();
        auto f2 = post_task();
        f1.wait();
        f2.wait();
    });
    std::thread t2([] {
        process_tasks();
    });
    t1.join();
    t2.join();
    return 0;
}
