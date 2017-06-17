#include <iostream>
#include <thread>

class Modifyable {
public:
    Modifyable()
    : a(0)
    {}
    void increment() {
        ++a;
    }
    void output() {
        std::cout << "a = " << a << "\n";
    }
private:
    int a;
};

void modify(Modifyable& modifyable) {
    modifyable.increment();
    std::cout << "Just after modifying: ";
    modifyable.output();
}

int main() {
    Modifyable modifyable;
    modify(modifyable);
    modifyable.output();
    std::thread t(modify, std::ref(modifyable));
    t.join();
    modifyable.output();
    return 0;
}
