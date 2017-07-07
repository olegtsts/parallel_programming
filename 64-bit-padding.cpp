#include <cstring>
#include <iostream>

struct CountedNodePtr {
    int external_count;
    char ch[4];
    int * c;
};
struct CountedNodePtr2 {
    int * c;
    char ch[4];
    int external_count;
};

struct CountedNodePtr3 {
    int * c;
    int external_count;
};

struct CountedNodePtr4 {
    int external_count;
    int * c;
};



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

    unsigned char ch[Bytes];
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
    CountedNodePtr a{1, "\0", nullptr};
    CountedNodePtr b{1, "\0", nullptr};
    std::cout << std::memcmp(&a, &b, sizeof(CountedNodePtr)) << " " << sizeof(CountedNodePtr) << std::endl;
    PointerWithCounter c{1, nullptr};
    PointerWithCounter d{1, nullptr};
    std::cout << std::memcmp(&c, &d, sizeof(PointerWithCounter)) << std::endl;
    int * e = nullptr;
    int * f = nullptr;
    std::cout << std::memcmp(&e, &f, sizeof(int*)) << std::endl;
    CountedNodePtr2 g{nullptr, "\0", 1};
    CountedNodePtr2 h{nullptr, "\0", 1};
    std::cout << std::memcmp(&g, &h, sizeof(CountedNodePtr2)) << " " << sizeof(CountedNodePtr2) << std::endl;
    CountedNodePtr3 j{nullptr,  1};
    CountedNodePtr4 k{1, nullptr};
    CountedNodePtr3 i{nullptr,  1};
    std::cout << std::memcmp(&j, &i, sizeof(CountedNodePtr3)) << " " << sizeof(CountedNodePtr3) << std::endl;
    CountedNodePtr4 l{1, nullptr};

    std::cout << std::memcmp(&k, &l, sizeof(CountedNodePtr3)) << " " << sizeof(CountedNodePtr3) << std::endl;
    return 0;
}
// output on osx
// 0 16
// 0
// 0
// 0 16
// 0 16
// 86 16
//
// output on linux
//0 16
//0
//0
//0 16
//-113 16
//0 16
//
//Last two memcmp operations have no exact guarantees on 64-bit systems
