#include <iostream>
#include <chrono>
#include <mutex>
#include <condition_variable>

int main() {
    std::condition_variable cv;
    std::mutex mu;
    auto final_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    std::unique_lock<std::mutex> lock(mu);
    while (true) {
        if (cv.wait_until(lock, final_time) == std::cv_status::timeout) {
            std::cout << "timeout reached\n";
            break;
        }
    }
    return 0;
}
