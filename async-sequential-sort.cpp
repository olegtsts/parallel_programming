#include <future>
#include <iostream>
#include <list>
#include <algorithm>

template <typename T>
std::list<T> parallel_quick_sort(std::list<T> input) {
    if (input.empty()) {
        return input;
    }
    std::list<T> result;
    result.splice(result.begin(), input, input.begin());
    const T& pivot = *result.begin();
    auto divide_point = std::partition(input.begin(), input.end(), [&](const T& t) {return t < pivot;});
    std::list<T> lower;
    lower.splice(lower.end(), input, input.begin(), divide_point);
    std::future<std::list<T> > new_lower(std::async(&parallel_quick_sort<T>, std::move(lower)));
    auto new_higher(parallel_quick_sort(std::move(input)));
    result.splice(result.end(), new_higher);
    result.splice(result.begin(), new_lower.get());
    return result;
}
int main() {
    std::list<int> l{4,2,3,1};
    auto another_list = parallel_quick_sort(l);
    for (auto& el: another_list) {
        std::cout << el;
        std::cout << std::endl;
    }
    return 0;
}
