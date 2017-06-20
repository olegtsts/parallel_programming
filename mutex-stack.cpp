#include <iostream>
#include <mutex>
#include <stack>
#include <thread>
#include <exception>
#include <memory>

template <typename T>
class MutexStack {
public:
    MutexStack() {}
    MutexStack(const MutexStack& other) {
        std::lock_guard<std::mutex> guard(mu);
        st = other.st;
    }
    MutexStack& operator=(const MutexStack&) = delete;
    void push(T new_value) {
        std::lock_guard<std::mutex> guard(mu);
        st.push(new_value);
    }
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> guard(mu);
        if (st.empty()) {
            throw std::logic_error("Empty stack");
        }
        std::shared_ptr<T> ptr(std::make_shared<T>(st.top()));
        st.pop();
        return ptr;
    }
    void pop(T& value) {
        std::lock_guard<std::mutex> guard(mu);
        if (st.empty()) {
            throw std::logic_error("Empty stack");
        }
        value = st.top();
        st.pop();
    }

    bool empty() const {
        std::lock_guard<std::mutex> guard(mu);
        return st.empty();
    }
private:
    std::mutex mu;
    std::stack<T> st;
};

int main() {
    MutexStack<int> stack;
    std::thread t1([&] {
        stack.push(1);
    });
    std::thread t2([&] {
        std::cout << *stack.pop() << std::endl;
    });
    t1.join();
    t2.join();
    return 0;
}
