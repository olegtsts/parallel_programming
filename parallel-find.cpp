#include <future>
#include <atomic>
#include <vector>
#include <thread>
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

template <typename Iterator, typename MatchType>
struct FindElement {
    void operator() (Iterator first_iter, Iterator last_iter, MatchType match,
            std::promise<Iterator>* result, std::atomic<bool>* done_flag) {
        try {
            for (;first_iter != last_iter && !done_flag->load(); ++first_iter) {
                if (*first_iter == match) {
                    result->set_value(first_iter);
                    done_flag->store(true);
                    return;
                }
            }
        } catch (...) {
            try {
                result->set_exception(std::current_exception());
                done_flag->store(true);
            } catch (...) {}
        }
    }
};

template <typename Iterator, typename MatchType>
Iterator parallel_find(Iterator first_iter, Iterator last_iter, MatchType match) {
    const size_t data_length = std::distance(first_iter, last_iter);
    if (!data_length) {
        return last_iter;
    }
    const size_t min_per_thread = 5;
    const size_t max_threads = (data_length + min_per_thread - 1) / min_per_thread;
    const size_t hardware_threads = std::thread::hardware_concurrency();
    const int num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    const size_t block_size = data_length / num_threads;
    std::promise<Iterator> result;
    std::atomic<bool> done_flag(false);
    std::vector<std::thread> threads(num_threads - 1);
    {
        JoinThreads joiner(threads);
        Iterator block_start = first_iter;
        for (int i = 0; i < num_threads - 1; ++i) {
            Iterator block_end = block_start;
            std::advance(block_end, block_size);
            threads[i] = std::thread(FindElement<Iterator, MatchType>(), block_start,
                    block_end, match, &result, &done_flag);
            block_start = block_end;
        }
        FindElement<Iterator, MatchType>()(block_start, last_iter, match, &result, &done_flag);
    }
    if (!done_flag.load()) {
        return last_iter;
    }
    return result.get_future().get();
}
int main() {
    std::vector<int> a{1,2,3,4};
    auto pointer = parallel_find(a.begin(), a.end(), 3);
    std::cout << *pointer << std::endl;
    return 0;
}
