#include <cassert>
#include <tuple>
#include <iostream>
#include <atomic>

template <typename T>
class PackedCountedPointer {
public:
    PackedCountedPointer()
    {}

    PackedCountedPointer(int counter, T * pointer)
    : content(pack(counter, pointer))
    {
    }

    int get_counter() {
        int counter;
        std::tie(counter, std::ignore) = unpack(content);
        return counter;
    }
    T * get_pointer() {
        T * pointer;
        std::tie(std::ignore, pointer) = unpack(content);
        return pointer;
    }
private:
    static long long int pack(int counter, T * pointer) {
        if (counter>= 30000) {
            throw std::logic_error("Too large counter");
        }
        long long int reintr_pointer = reinterpret_cast<long long int>(pointer);
        return (reintr_pointer | (static_cast<long long int>(counter) << 48));
    }
    static std::tuple<int, T*> unpack(long long int packed) {
        int counter = packed >> 48;
        long long int reintr_pointer = packed & ~(static_cast<long long int>(counter) << 48);
        T * pointer = reinterpret_cast<T *>(reintr_pointer);
        return std::make_tuple(counter, pointer);
    }

    long long int content;
};


int main() {
    std::atomic<PackedCountedPointer<double> > packed(PackedCountedPointer<double>(123, new double (0.5)));
    auto data = packed.load();
    std::cout << std::boolalpha << data.get_counter()
        << " " << *data.get_pointer() << " " << packed.is_lock_free() << std::endl;
    return 0;
}
