#include <type_traits>
#include <iostream>
#include <vector>

class Base {
public:
    virtual void OutputIsNoexcept() = 0 ;
    virtual ~Base() {}
};
template <typename F>
class A : public Base {
public:
    virtual void OutputIsNoexcept() {
        std::cout << std::is_nothrow_invocable_v<decltype(&F::Func), F> << std::endl;
    }
    virtual ~A() {}
};
class B : public A<B> {
public:
    void Func() noexcept {
    }
};

class C : public A<C> {
public:
    void Func() {
    }
};

int main(){
    std::vector<Base*> vec{new B(), new C()};
    for (auto& pointer: vec) {
        pointer->OutputIsNoexcept();
    }
}
