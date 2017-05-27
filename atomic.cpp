#include <iostream>
#include <atomic>

template <typename T>
void output_bool(const T& var, const std::string& name) {
    std::cout << name << " = ";
    if (var) {
        std::cout << "True";
    } else {
        std::cout << "False";
    }
    std::cout << std::endl;
}

int main() {
    std::atomic_bool flag(true);
    bool expected = true;
    output_bool(flag, "flag");
    output_bool(expected, "expected");
    bool result = flag.compare_exchange_weak(expected, false);
    output_bool(flag, "flag");
    output_bool(expected, "expected");
    output_bool(result, "result");
    return 0;
}
