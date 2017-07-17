#include <iostream>
#include <vector>
#include <algorithm>
#include <queue>

template <typename T>
class TypeSpecifier {};

template <typename ... Args>
class Numerator {
public:
    int counter;

    Numerator()
    : counter(0)
    {}

    void GetCounterImpl() {}

    virtual ~Numerator() {}
};


template <typename T, typename ... Args>
class Numerator<T, Args...> : public Numerator<Args...> {
private:
    int own_counter;
public:
    using Numerator<Args...>::counter;
    using Numerator<Args...>::GetCounterImpl;

    Numerator()
    : Numerator<Args...>()
    {
        own_counter = counter++;
    }

    int GetCounterImpl(TypeSpecifier<T>) {
        return own_counter;
    }

    template <typename T2>
    int GetCounter() {
        return GetCounterImpl(TypeSpecifier<T2>());
    }

    virtual ~Numerator() {}
};

template <template <typename TPiper> class From, template <typename TPiper> class To, typename Message>
class Edge{};

template <template <typename T2> class T>
class TemplateTypeSpecifier {};

template <typename ... Args>
class Piper {
protected:
    int max_to_index;
    int max_edge_index;
    std::vector<std::vector<int>> dest_pipes;
    std::vector<std::queue<void*>> queues;
public:
    Piper()
    : max_to_index(0)
    , max_edge_index(0)
    {
    }

    template <template <typename TPiper> class To>
    int GetToIndexImpl(const TemplateTypeSpecifier<To>&) {
        return -1;
    }

    void GetEdgeIndexImpl() {}
    void GetAssosiatedEdgesImpl() {}
    void PushMessageToQueueImpl() {}
};

template <template <typename TPiper> class From, template <typename TPiper> class To, typename Message, typename ... Args>
class Piper<Edge<From, To, Message>, Args...> : public Piper<Args...> {
private:
    int cur_to_index;
    int cur_edge_index;

    template <template <typename TPiper> class From2>
    class SenderProxy {
    private:
        Piper& piper;
    public:
        SenderProxy(Piper& piper)
        : piper(piper)
        {}

        template <template <typename TPiper> class To2, typename Message2>
        void Send(Message2 * message) {
            piper.PushMessageToQueueImpl(TemplateTypeSpecifier<From2>(), TemplateTypeSpecifier<To>(), message);
        }
    };
public:
    using Piper<Args...>::max_to_index;
    using Piper<Args...>::GetToIndexImpl;
    using Piper<Args...>::GetEdgeIndexImpl;
    using Piper<Args...>::max_edge_index;
    using Piper<Args...>::dest_pipes;
    using Piper<Args...>::GetAssosiatedEdgesImpl;
    using Piper<Args...>::PushMessageToQueueImpl;
    using Piper<Args...>::queues;
    Piper()
    : Piper<Args...>()
    {
        cur_to_index = Piper<Args...>::GetToIndexImpl(TemplateTypeSpecifier<To>());
        if (cur_to_index == -1) {
            cur_to_index = max_to_index++;
            dest_pipes.push_back(std::vector<int>());
        }
        cur_edge_index = max_edge_index++;
        dest_pipes[cur_to_index].push_back(cur_edge_index);
        queues.push_back(std::queue<void*>());
    }
    int GetToIndexImpl(const TemplateTypeSpecifier<To>&) {
        return cur_to_index;
    }
    int GetEdgeIndexImpl(const TypeSpecifier<Edge<From,To,Message>>&) {
        return cur_edge_index;
    }
    std::vector<int> GetAssosiatedEdgesImpl(const TemplateTypeSpecifier<To>& specifier) {
        return dest_pipes[GetToIndexImpl(specifier)];
    }
    void PushMessageToQueueImpl(const TemplateTypeSpecifier<From>&, const TemplateTypeSpecifier<To>&, Message * message) {
        queues[cur_edge_index].push(static_cast<void*>(message));
    }
};


template <typename TPiper>
class MessageProcessorBase {
protected:
    TPiper& piper;
public:
    MessageProcessorBase(TPiper& piper)
    : piper(piper)
    {}
};

template <typename TPiper>
class MessageProcessorA;

template <typename TPiper>
class MessageProcessorB;

template <typename TPiper>
class MessageProcessorA : public MessageProcessorBase<TPiper> {
public:
    using MessageProcessorBase<TPiper>::MessageProcessorBase;

    void Receive(TemplateTypeSpecifier<MessageProcessorB>&, double& value) {
        std::cout << "MessageProcessorA: I have got int value from MessageProcessorB " << value << std::endl;
    }
};

template <typename TPiper>
class MessageProcessorB : public MessageProcessorBase<TPiper> {
public:
    using MessageProcessorBase<TPiper>::MessageProcessorBase;
    using MessageProcessorBase<TPiper>::piper;

    void Receive(TemplateTypeSpecifier<MessageProcessorA>&, int& value) {
        std::cout << "MessageProcessorB: I have got int value from MessageProcessorA " << value << std::endl;
        piper.template Send<MessageProcessorB>(new double(1));
    }
    void Receive(TemplateTypeSpecifier<MessageProcessorA>&, double& value) {
        std::cout << "MessageProcessorB: I have got double value from MessageProcessorA"  << value << std::endl;
    }
};


template <typename T>
class C1 {};
template <typename T>
class C2 {};

int main(){
//    Numerator<int, double, float> numerator;
//    std::cout << numerator.GetCounter<double>() << " " << numerator.GetCounter<int>() << " " << numerator.GetCounter<float>()  << std::endl;
    Piper<Edge<C1, C1, int>, Edge<C2, C2, double>, Edge<C2, C2, int>> piper;
    std::cout << piper.GetToIndexImpl(TemplateTypeSpecifier<C1>()) << " " << piper.GetToIndexImpl(TemplateTypeSpecifier<C2>()) << std::endl;
    std::cout << piper.GetEdgeIndexImpl(TypeSpecifier<Edge<C1, C1, int>>()) << std::endl;
    auto edges = piper.GetAssosiatedEdgesImpl(TemplateTypeSpecifier<C2>());
    std::for_each(edges.begin(), edges.end(), [] (const auto& edge) {std::cout << " " << edge;});
    std::cout << std::endl;
    edges = piper.GetAssosiatedEdgesImpl(TemplateTypeSpecifier<C1>());
    std::for_each(edges.begin(), edges.end(), [] (const auto& edge) {std::cout << " " << edge;});
    std::cout << std::endl;

    return 0;
}
