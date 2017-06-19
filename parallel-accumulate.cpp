#include <iostream>
#include <numeric>
#include <iterator>
#include <algorithm>
#include <thread>

template <typename Iterator, typename TValue>
class BlockCalculation {
public:
    void operator()(Iterator begin, Iterator end, TValue& result) {
        std::cout << "Starting\n";
        result = std::accumulate(begin, end, 0);
        std::cout << "Done\n";
    }
};

template <typename Iterator, typename TValue>
TValue parallel_accumulate(Iterator first, Iterator last, TValue init) {
    size_t length = std::distance(first, last);
    if (length == 0) {
        return init;
    }
    size_t min_per_thread = 5;
    size_t max_threads = (length + min_per_thread - 1) / min_per_thread;
    size_t hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads == 0) {
        hardware_threads = 2;
    }
    size_t num_threads = std::min(hardware_threads, max_threads);
    size_t block_size = length / num_threads;
    std::vector<TValue> results(num_threads);
    std::vector<std::thread> threads(static_cast<int>(num_threads) - 1);
    Iterator current = first;
    for (size_t i = 0; i + 1 < num_threads; ++i) {
        Iterator current_end = current;
        std::advance(current_end, block_size);
        threads[i] = std::thread(BlockCalculation<Iterator, TValue>(), current, current_end, std::ref(results[i]));
        current = current_end;
    }
    BlockCalculation<Iterator, TValue>()(current, last, std::ref(results[static_cast<int>(num_threads) - 1]));
    std::for_each(threads.begin(), threads.end(), std::mem_fn(&std::thread::join));
    return std::accumulate(results.begin(), results.end(), init);
}

int main() {
    std::vector<int> numbers{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,19,20,21,22,23,24,25,26,27,28};
    std::cout << "Result value = " << parallel_accumulate(numbers.begin(), numbers.end(), 16) << "\n";
    return 0;
}
