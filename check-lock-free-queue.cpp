#include <cstdlib>
#include <iostream>

int main() {
    const int each_output = 100;
    int tests_passed = 0;
    while (true) {
        if (system("./queue.out 1> out 2>&1")) {
            throw std::logic_error("Tested programm failed");
        }
        if (system("bash -c 'diff <(grep Allocate out | sed \"s/.*Allocated pointer//g\"  |  sort) <(grep Deallocate out | sed \"s/.*Deallocated pointer//g\" | sort)'")) {
            throw std::logic_error("Test failed");
        }
        ++tests_passed;
        if (tests_passed % each_output == 0) {
            system("date");
            std::cout << "Passed " << tests_passed << "\n";
        }
    }
    return 0;
}
