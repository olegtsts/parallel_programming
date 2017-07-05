#include <iterator>
#include <atomic>
#include <future>
#include <functional>
#include <iostream>
#include <vector>

template <typename Iterator, typename MatchType>
Iterator parallel_find_impl(Iterator first_iter, Iterator last_iter, MatchType match,
        std::atomic<bool>& done) {
    try {
        const size_t data_length = std::distance(first_iter, last_iter);
        const size_t min_per_thread = 25;
        if (data_length < 25 * min_per_thread) {
            for (; (first_iter != last_iter) && !done.load(); ++first_iter) {
                if (*first_iter == match) {
                    done = true;
                    return first_iter;
                }
            }
            return last_iter;
        } else {
            Iterator mid_point = first_iter;
            std::advance(mid_point, data_length / 2);
            std::future<Iterator> async_result = std::async(&parallel_find_impl<Iterator, MatchType>,
                    mid_point, last_iter, match, std::ref(done));
            const Iterator direct_result = parallel_find_impl(first_iter, mid_point, match, done);
            return (direct_result == mid_point) ? async_result.get() : direct_result;
        }
    } catch (...) {
        done = true;
        throw;
    }
}

template <typename Iterator, typename MatchType>
Iterator parallel_find(Iterator first_iter, Iterator last_iter, MatchType match) {
    std::atomic<bool> done(false);
    return parallel_find_impl(first_iter, last_iter, match, done);
}
int main() {
    std::vector<int> a{1,2,3,4};
    auto pointer = parallel_find(a.begin(), a.end(), 3);
    std::cout << *pointer << std::endl;
    return 0;
}
