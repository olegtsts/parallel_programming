#include <atomic>
#include <thread>
#include <vector>
#include <iterator>
#include <functional>
#include <iostream>
#include <algorithm>

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

class Barrier {
public:
    explicit Barrier(const int waiting_count)
    : waiting_count(waiting_count)
    , spaces(waiting_count)
    , generation(0)
    {}

    void Wait() {
        const int my_generation = generation;
        if (!--spaces) {
            spaces = waiting_count.load();
            ++generation;
        } else {
            while (generation == my_generation) {
                std::this_thread::yield();
            }
        }
    }
    void DoneWaiting() {
        --waiting_count;
        if (!--spaces) {
            spaces = waiting_count.load();
            ++generation;
        }
    }
private:
    std::atomic<int> waiting_count;
    std::atomic<int> spaces;
    std::atomic<int> generation;
};

template <typename Iterator>
void parallel_partial_sum(Iterator first_iter, Iterator last_iter) {
    typedef typename Iterator::value_type ValueType;

    struct ProcessElement {
        void operator() (Iterator first_iter, Iterator last_iter,
                std::vector<ValueType>& buffer,
                size_t i, Barrier& b) {
            ValueType& ith_element = *(first_iter + i);
            bool update_source = false;
            for (size_t step = 0, stride = 1; stride <= i; ++step, stride *= 2) {
                const ValueType& source = (step % 2) ? buffer[i] : ith_element;
                ValueType& dest = (step % 2) ? ith_element:buffer[i];
                const ValueType& addend = (step % 2) ? buffer[i - stride] : *(first_iter + i - stride);
                dest = source + addend;
                update_source = !(step % 2);
                b.Wait();
            }
            if (update_source) {
                ith_element = buffer[i];
            } else {
                buffer[i] = ith_element;
            }
            b.DoneWaiting();
        }
    };

    const int data_length = std::distance(first_iter, last_iter);
    if (!data_length) {
        return;
    }
    std::vector<ValueType> buffer(data_length);
    Barrier b(data_length);
    std::vector<std::thread> threads(data_length - 1);
    JoinThreads joiner(threads);
    for (int i = 0; i < data_length - 1; ++i) {
        threads[i] = std::thread(ProcessElement(), first_iter, last_iter,
                std::ref(buffer), i, std::ref(b));
    }
    ProcessElement()(first_iter, last_iter, buffer, data_length - 1, b);
}
int main() {
    std::vector<int> a{1,2,3,4,5,6,7,8,9,10};
    parallel_partial_sum(a.begin(), a.end());
    std::for_each(a.begin(), a.end(), [&](const int el) {std::cout << " " << el;});
    std::cout << std::endl;
    return 0;
}
