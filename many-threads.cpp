#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstdio>

void do_work(const size_t i) {
    printf("I am thread %lu\n", i);
}

int main() {
    std::vector<std::thread> threads;
    for (size_t i = 0; i < 20; ++i ){
        threads.push_back(std::thread(do_work, i));
    }
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    return 0;
}
