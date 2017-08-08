#include <iostream>
#include <list>

int main() {
    std::list<int> list1 = { 1, 2, 3, 4, 5 };
    std::list<int> list2 = { 10, 20, 30, 40, 50 };
    auto it = list1.begin();
    std::cout << *it << " " << (it == list1.begin()) << std::endl;
    list1.splice(list1.begin(), std::move(list2));
    std::cout << *it << " " << (it == list1.begin()) << std::endl;
    std::cout << "list 1 = ";
    for (auto& el: list1) {
        std::cout << el << " ";
    }
    std::cout << std::endl;
    std::cout << "list 2 = ";
    for (auto& el: list2) {
        std::cout << el << " ";
    }
    std::cout << std::endl;
    return 0;
}
