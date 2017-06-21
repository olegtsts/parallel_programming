#include <iostream>
#include <mutex>
#include <thread>

void do_work() {
    std::cout << "Hello\n";
}

std::once_flag flag;
void thread_job() {
    std::call_once(flag, do_work);
}
int main () {
    std::thread t1(thread_job);
    std::thread t2(thread_job);
    t1.join();
    t2.join();
    return 0;
}
