#include <iterator>
#include <future>
#include <iostream>
#include <vector>
#include <algorithm>

template <typename Iterator, typename TValue>
TValue parallel_accumulate(Iterator first, Iterator last, TValue init) {
    size_t length = std::distance(first, last);
    size_t max_chunk_size = 25;
    if (length <= max_chunk_size) {
        return std::accumulate(first, last, init);
    } else {
        Iterator mid_point = first;
        std::advance(mid_point, length / 2);
        std::future<TValue> first_half_result = std::async(parallel_accumulate<Iterator, TValue>,
                first, mid_point, init);
        TValue second_half_result = parallel_accumulate(mid_point, last, TValue());
        return first_half_result.get() + second_half_result;
    }
}

int main() {
    std::vector<int> numbers{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,17,18,19,20,21,22,23,24,25,26,27,28};
    std::cout << "Result value = " << parallel_accumulate(numbers.begin(), numbers.end(), 16) << "\n";
    return 0;
}
