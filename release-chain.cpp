#include <cstdlib>
#include <vector>
#include <iostream>
#include <atomic>
#include <thread>
#include <sstream>

std::vector<int> data;
std::atomic<int> count;

void fill_vector() {
    int number_of_items = 20;
    data.clear();
    for (int i = 0 ; i < number_of_items; ++i) {
        data.push_back(i);
    }
    count.store(number_of_items, std::memory_order_release);
}
void consume_queue_items() {
    while (true) {
        int index;
        if ((index = count.fetch_sub(1, std::memory_order_acquire)) <= 0) {
            continue;
        }
        std::stringstream ss;
        ss << std::this_thread::get_id() << " : " << data[index - 1] << std::endl;
        std::cout << ss.str();
    }
}
int main() {
    std::thread t1(fill_vector);
    std::thread t2(consume_queue_items);
    std::thread t3(consume_queue_items);
    t1.join();
    t2.join();
    t3.join();
    return 0;
}
