#include <cassert>
#include <tuple>
#include <iostream>
#include <atomic>
#include <vector>

template <typename T, size_t Bytes, typename TStore=T>
class Packed {
public:
    Packed()
    {
    }
    Packed(const T& value)
    {
        save_bytes(value, ch, Bytes);
    }
    operator T() {
        return load_bytes(ch, Bytes);
    }
private:
    void save_bytes(const T& source_value, unsigned char * ch, const size_t bytes) {
        TStore value = reinterpret_cast<TStore>(source_value);
        long long int mask = (1 << 8) - 1;
        for (size_t i = 0; i < bytes; ++i) {
            ch[i] = value & mask;
            value = value >> 8;
        }
    }

    T load_bytes(unsigned char * ch, const size_t bytes) {
        TStore value = 0;
        for (size_t i = 0; i < bytes; ++i) {
            value = value << 8;
            value |= ch[static_cast<int>(bytes) - i - 1];
        }
        return reinterpret_cast<T>(value);
    }

    unsigned char ch[Bytes];
};

template <typename T>
class PackedPointer : public Packed<T*, 6, long long int> {
    using Packed<T*, 6, long long int>::Packed;
};

class Int16: public Packed<int, 2>{
    using Packed<int, 2>::Packed;
};

struct PointerWithCounter {
    Int16 a;
    PackedPointer<int> p;
};

int main() {
    std::atomic<PointerWithCounter> p{{123, new int (456)}};
    std::cout << p.load().a << " " << *p.load().p << " " << std::boolalpha << p.is_lock_free() << std::endl;
    return 0;
}
