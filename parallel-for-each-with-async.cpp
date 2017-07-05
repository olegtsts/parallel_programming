#include <algorithm>
#include <future>
#include <iterator>
#include <thread>
#include <sstream>
#include <iostream>

template <typename Iterator, typename Func>
void parallel_for_each(Iterator first_iter, Iterator last_iter, Func f) {
    const size_t data_length = std::distance(first_iter, last_iter);
    if (!data_length) {
        return;
    }
    const size_t min_per_thread = 5;
    if (data_length < 2 * min_per_thread) {
        std::for_each(first_iter, last_iter, f);
    } else {
        Iterator mid_point = first_iter + (data_length / 2);
        std::future<void> first_half = std::async(&parallel_for_each<Iterator, Func>,
                first_iter, mid_point, f);
        parallel_for_each(mid_point, last_iter, f);
        first_half.get();
    }
}
int main() {
    std::vector<int> a{1,2,3,4,5,6,7,8,9,10,2,3,4,5,6,7,8,9,10};
    parallel_for_each(a.begin(), a.end(), [] (const int element) {
        std::stringstream ss;
        ss << std::this_thread::get_id() << " " << element << std::endl;
        std::cout << ss.str();
    });
    return 0;
}
