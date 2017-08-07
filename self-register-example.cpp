#include <vector>
#include <iostream>

class Exception {
public:
    Exception();

    void Func() const noexcept {
        std::cout << "Func" << std::endl;
    }
};

class Keeper {
public:
    void AddException(const Exception& exception) {
        exceptions.push_back(exception);
    }
    const std::vector<Exception>& GetExceptions() const {
        return exceptions;
    }
private:
    std::vector<Exception> exceptions;
};

thread_local Keeper keeper;

Exception::Exception() {
    keeper.AddException(*this);
}

int main() {
    Exception first_exception;
    Exception second_exception;

    for(auto& ex: keeper.GetExceptions()) {
        ex.Func();
    }

    return 0;
}
