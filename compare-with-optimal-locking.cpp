#include <iostream>
#include <mutex>
#include <thread>

class Some {
public:
    Some(const int content)
    : content(content)
    {}

    int get_content() const {
        std::lock_guard<std::mutex> lock(mu);
        return content;
    }

    bool operator==(const Some& other) {
        if (this == &other) {
            return true;
        }
        int first_value = get_content();
        int second_value = other.get_content();
        return first_value == second_value;
    }
private:
    int content;
    mutable std::mutex mu;
};
int main() {
    Some a(1);
    Some b(1);
    std::thread t1([&] {
        std::cout << (a == b ? "True\n" : "False\n");
    });
    t1.join();
    return 0;
}
