#include <iostream>
#include <functional>
#include <vector>
#include <thread>

void function(int a) {
    std::cout << std::this_thread::get_id() << " " << a << std::endl;
}

template <typename Func, typename T>
void multi_execute(Func function, const std::vector<T>& values) {
    for (auto& value:values) {
        function(value);
    }
}

template <typename Func, typename Iterator, typename T>
void execute_parallel(Iterator begin, Iterator end, Func function, const int threadnum) {
    size_t index = 0;
    std::vector<std::vector<T> > values(threadnum);
    for (auto it = begin; it != end; ++it) {
        size_t thread_idnex = index++ % threadnum;
        values[thread_idnex].push_back(*it);
    }
    std::vector<std::thread> threads(threadnum);
    for (int i = 0; i < threadnum; ++i) {
        threads[i] = std::thread(multi_execute<Func, T>, function, values[i]);
    }
    for (int i = 0; i < threadnum; ++i) {
        threads[i].join();
    }
}

int main() {
    std::vector<int> vect = {1,2,3,4,5,6,7};

    execute_parallel<std::function<void(int)>, std::vector<int>::iterator, int>(vect.begin(), vect.end(), function, 2);

    return 0;
}
