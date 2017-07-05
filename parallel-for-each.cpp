#include <iterator>
#include <vector>
#include <future>
#include <thread>
#include <sstream>
#include <algorithm>
#include <iostream>

class JoinThreads {
public:
    explicit JoinThreads(std::vector<std::thread>& threads)
    : threads(threads)
    {}

    ~JoinThreads() {
        for (auto& cur_thread: threads) {
            if (cur_thread.joinable()) {
                cur_thread.join();
            }
        }
    }
private:
    std::vector<std::thread>& threads;
};

template <typename Iterator, typename Func>
void parallel_for_each(Iterator first_iter, Iterator last_iter, Func f) {
    const size_t data_length = std::distance(first_iter, last_iter);
    if (!data_length) {
        return;
    }
    const size_t min_per_thread = 5;
    const size_t max_threads = (data_length + min_per_thread - 1) / min_per_thread;
    const size_t hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    const size_t block_size = data_length / num_threads;
    std::vector<std::future<void>> futures(num_threads - 1);
    std::vector<std::thread> threads(num_threads - 1);
    JoinThreads joiner(threads);
    Iterator block_start = first_iter;
    for (int i = 0; i < num_threads - 1; ++i) {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        std::packaged_task<void(void)> task([=] {
            std::for_each(block_start, block_end, f);
        });
        futures[i] = task.get_future();
        threads[i] = std::thread(std::move(task));
        block_start = block_end;
    }
    std::for_each(block_start, last_iter, f);
    for (int i = 0; i < num_threads - 1; ++i) {
        futures[i].get();
    }
}
int main() {
    std::vector<int> a{1,2,3};
    parallel_for_each(a.begin(), a.end(), [] (const int element) {
        std::stringstream ss;
        ss << element << std::endl;
        std::cout << ss.str();
    });
    return 0;
}
