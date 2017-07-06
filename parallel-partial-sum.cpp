#include <algorithm>
#include <numeric>
#include <future>
#include <iterator>
#include <vector>
#include <iostream>
#include <sstream>

class JoinThreads {
public:
    explicit JoinThreads(std::vector<std::thread>& threads)
    : threads(threads)
    {}

    ~JoinThreads() {
        for (auto& thread: threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
private:
    std::vector<std::thread>& threads;
};

template <typename Iterator>
void parallel_partial_sum(Iterator first_iter, Iterator last_iter) {
    typedef typename Iterator::value_type ValueType;
    struct ProcessChunk {
        void operator() (Iterator first_iter, Iterator last_iter,
                std::future<ValueType>* previous_end_value,
                std::promise<ValueType>* end_value) {
            std::stringstream ss;
            ss << "Processing chunk from thread" << std::this_thread::get_id() << "\n";
            std::cout << ss.str();
            try {
                Iterator end_iter = last_iter;
                ++end_iter;
                std::partial_sum(first_iter, end_iter, first_iter);
                if (previous_end_value) {
                    ValueType addend = previous_end_value->get();
                    *last_iter += addend;
                    if (end_value) {
                        end_value->set_value(*last_iter);
                    }
                    std::for_each(first_iter, last_iter, [addend](ValueType& item) {
                        item += addend;
                    });
                } else if (end_value) {
                    end_value->set_value(*last_iter);
                }
            } catch (...) {
                if (end_value) {
                    end_value->set_exception(std::current_exception());
                } else {
                    throw;
                }
            }
        }
    };

    const size_t data_length = std::distance(first_iter, last_iter);
    if (!data_length) {
        return;
    }
    size_t min_per_thread = 5;
    size_t max_threads = (data_length + min_per_thread - 1) / min_per_thread;
    size_t hardware_threads = std::thread::hardware_concurrency();
    if (hardware_threads == 0) {
        hardware_threads = 2;
    }
    int num_threads = std::min(hardware_threads, max_threads);
    size_t block_size = data_length / num_threads;

    std::vector<std::thread> threads(num_threads - 1);
    std::vector<std::promise<ValueType>> end_values(num_threads - 1);
    std::vector<std::future<ValueType>> previous_end_values;
    previous_end_values.reserve(num_threads - 1);
    JoinThreads joiner(threads);
    Iterator block_start = first_iter;
    for (int i = 0; i < num_threads - 1; ++i) {
        Iterator block_last = block_start;
        std::advance(block_last, block_size - 1);
        threads[i] = std::thread(ProcessChunk(),
                block_start, block_last,
                (i != 0) ? &previous_end_values[i - 1] : 0,
                &end_values[i]);
        block_start = block_last;
        ++block_start;
        previous_end_values.push_back(end_values[i].get_future());
    }
    Iterator final_element = block_start;
    std::advance(final_element, std::distance(block_start, last_iter) - 1);
    ProcessChunk()(block_start, final_element, (num_threads > 1) ? &previous_end_values.back() : 0,
            0);
}
int main() {
    std::vector<int> a{1,2,3,4,5,6,7,8,9,10};
    parallel_partial_sum(a.begin(), a.end());
    std::for_each(a.begin(), a.end(), [&](const int el) {std::cout << " " << el;});
    std::cout << std::endl;
    return 0;
}
