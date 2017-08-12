#include <iostream>

template <typename TState, typename ... Args>
class ApplicableTo: public ApplicableTo<TState>, public ApplicableTo<Args...> {
public:
    using ApplicableTo<TState>::ApplyTo;
    using ApplicableTo<Args...>::ApplyTo;
};

template <typename TState>
class ApplicableTo<TState> {
public:
    virtual void ApplyTo(TState& state) {}
};

class State {
public:
    friend class FirstUpdate;
    friend class SecondUpdate;
private:
    int a;
};

class SecondState {
public:
};


class FirstUpdate : public ApplicableTo<State, SecondState> {
public:
    void ApplyTo(State& state) {
        std::cout << "FirstUpdate\n";
        state.a = 0;
    }
    void ApplyTo(SecondState& state) {
        std::cout << "FirstUpdate to SecondState\n";
    }
};

class SecondUpdate : public ApplicableTo<State, SecondState> {
    void ApplyTo(State& state) {
        std::cout << "SecondUpdate\n";
        state.a = 1;
    }
};

int main() {
    State state;
    ApplicableTo<State>* update = new FirstUpdate();
    update->ApplyTo(state);
    ApplicableTo<State>* second_update = new SecondUpdate();
    second_update->ApplyTo(state);
    ApplicableTo<SecondState>* third_update = new FirstUpdate();
    SecondState second_state;
    third_update->ApplyTo(second_state);
}
