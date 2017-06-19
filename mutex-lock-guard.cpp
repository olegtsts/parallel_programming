#include <iostream>
#include <thread>
#include <mutex>
#include <list>
#include <algorithm>

std::list<int> some_list;
std::mutex some_mutex;
void add_to_list(int const new_value) {
    std::lock_guard<std::mutex> guard(some_mutex);
    some_list.push_back(new_value);
}
bool list_contains(int const value) {
    std::lock_guard<std::mutex> guard(some_mutex);
    return std::find(some_list.begin(), some_list.end(), value) != some_list.end();
}
int main() {
    std::thread t1([] {
            add_to_list(3);
    });
    std::thread t2([] {
            std::cout << (list_contains(3) ? "Contains\n" : "Not contains\n");
    });
    t1.join();
    t2.join();
    return 0;
}
