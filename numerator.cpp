#include <iostream>
#include <vector>
#include <deque>
#include <memory>

template <typename T>
class TypeSpecifier {};

template <typename From, typename To, typename Message>
class Edge{};

class MessageProcessorBase {
public:
    virtual ~MessageProcessorBase(){}
};

class MessageBase {
public:
    virtual ~MessageBase() {}
};

template <typename ... Args>
class Piper {
protected:
    template <typename GlobalPiper>
    class ReceiveHandler {
    public:
        virtual void CallReceive(GlobalPiper * piper) = 0;
        virtual ~ReceiveHandler() {}
    };
    int max_to_index;
    int max_edge_index;
    std::vector<std::vector<int>> dest_pipes;
    std::vector<std::deque<std::unique_ptr<MessageBase>>> queues;
    std::vector<std::unique_ptr<MessageProcessorBase>> message_processors;
public:
    Piper()
    : max_to_index(0)
    , max_edge_index(0)
    {
    }

    template <typename To>
    int GetToIndexImpl(const TypeSpecifier<To>&) {
        return -1;
    }

    template <typename GlobalPiper>
    void FillReceiveHandlersImpl(std::vector<std::unique_ptr<Piper::ReceiveHandler<GlobalPiper>>>&) {
    }
    void GetEdgeIndexImpl() {}
    void GetAssosiatedEdgesImpl() {}
    void PushMessageToQueueImpl() {}

    virtual ~Piper() {}
};

template <typename From, typename To, typename Message, typename ... Args>
class Piper<Edge<From, To, Message>, Args...> : public Piper<Args...> {
private:
    int cur_to_index;
    int cur_edge_index;
    std::vector<std::unique_ptr<Piper<>::ReceiveHandler<Piper>>> receive_handlers;

    template <typename From2>
    class SenderProxy {
    private:
        Piper& piper;
    public:
        SenderProxy(Piper& piper)
        : piper(piper)
        {}

        template <typename To2, typename Message2>
        void Send(std::unique_ptr<Message2> message) const {
            piper.PushMessageToQueueImpl(TypeSpecifier<From2>(), TypeSpecifier<To2>(), std::move(message));
        }
    };
protected:
    template <typename GlobalPiper>
    class ReceiveHandler : public Piper<Args...>::template ReceiveHandler<GlobalPiper> {
    public:
        using Piper<Args...>::template ReceiveHandler<GlobalPiper>::ReceiveHandler;

        virtual void CallReceive(GlobalPiper * piper) {
            auto cur_parent = dynamic_cast<Piper *>(piper);
            To* message_processor = dynamic_cast<To*>(cur_parent->message_processors[cur_parent->cur_to_index].get());
            auto& current_queue = cur_parent->queues[cur_parent->cur_edge_index];
            Message* value = dynamic_cast<Message*>(current_queue.front().get());
            current_queue.pop_front();
            message_processor->Receive(TypeSpecifier<From>(), *value, piper->GetSenderProxy<To>());
        }
        virtual ~ReceiveHandler() {}
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
    using Piper<Args...>::message_processors;
    Piper()
    : Piper<Args...>()
    {
        cur_to_index = Piper<Args...>::GetToIndexImpl(TypeSpecifier<To>());
        if (cur_to_index == -1) {
            cur_to_index = max_to_index++;
            dest_pipes.push_back(std::vector<int>());
            message_processors.push_back(std::make_unique<To>());
        }
        cur_edge_index = max_edge_index++;
        dest_pipes[cur_to_index].push_back(cur_edge_index);
    }
    template <typename GlobalPiper>
    void FillReceiveHandlersImpl(std::vector<std::unique_ptr<Piper<>::ReceiveHandler<GlobalPiper>>>& handlers) {
        Piper<Args...>::template FillReceiveHandlersImpl<GlobalPiper>(handlers);
        handlers.push_back(std::make_unique<ReceiveHandler<GlobalPiper>>());
    }
    void LastLevelInit() {
        FillReceiveHandlersImpl<Piper>(receive_handlers);
        queues = std::vector<std::deque<std::unique_ptr<MessageBase>>>(max_edge_index);
    }

    int GetToIndexImpl(const TypeSpecifier<To>&) {
        return cur_to_index;
    }
    int GetEdgeIndexImpl(const TypeSpecifier<Edge<From,To,Message>>&) {
        return cur_edge_index;
    }
    std::vector<int> GetAssosiatedEdgesImpl(const TypeSpecifier<To>& specifier) {
        return dest_pipes[GetToIndexImpl(specifier)];
    }
    void PushMessageToQueueImpl(const TypeSpecifier<From>&, const TypeSpecifier<To>&, std::unique_ptr<Message> message) {
        queues[cur_edge_index].push_back(std::move(message));
    }

    void CallReceive(const int index) {
        receive_handlers[index].get()->CallReceive(this);
    }

    template <typename From2>
    SenderProxy<From2> GetSenderProxy() {
        return SenderProxy<From2>(*this);
    }

    template <typename From2,typename To2, typename Message2>
    void SendFromTo(std::unique_ptr<Message2> message) {
        auto sender = GetSenderProxy<From2>();
        sender.template Send<To2>(std::move(message));
    }

    virtual ~Piper() {}
};

template <typename ... Args>
class MessagePassingTree : public Piper<Args...> {
public:
    using Piper<Args...>::SendFromTo;

    MessagePassingTree()
    : Piper<Args...>()
    {
        Piper<Args...>::LastLevelInit();
    }
};

class MessageProcessorA;

class MessageProcessorB;

class DoubleMessage : public MessageBase {
public:
    DoubleMessage(const double a)
    : a(a)
    {}
    double a;
};
class IntMessage: public MessageBase {
public:
    IntMessage(const int a)
    : a(a)
    {}
    double a;
};

class MessageProcessorA : public MessageProcessorBase {
public:
    using MessageProcessorBase::MessageProcessorBase;

    template <typename Sender>
    void Receive(const TypeSpecifier<MessageProcessorB>&, const DoubleMessage& value, const Sender& sender) {
        std::cout << "MessageProcessorA: I have got int value from MessageProcessorB " << value.a << std::endl;
        sender.template Send<MessageProcessorB>(std::make_unique<IntMessage>(2 + static_cast<int>(value.a)));
    }
    virtual ~MessageProcessorA(){}
};

class MessageProcessorB : public MessageProcessorBase {
public:
    using MessageProcessorBase::MessageProcessorBase;

    template <typename Sender>
    void Receive(const TypeSpecifier<MessageProcessorA>&, const IntMessage& value, const Sender& sender) {
        std::cout << "MessageProcessorB: I have got int value from MessageProcessorA " << value.a << std::endl;
        sender.template Send<MessageProcessorA>(std::make_unique<DoubleMessage>(3 + value.a));
    }
    template <typename Sender>
    void Receive(const TypeSpecifier<MessageProcessorA>&, const DoubleMessage& value, const Sender&) {
        std::cout << "MessageProcessorB: I have got double value from MessageProcessorA"  << value.a << std::endl;
    }
    virtual ~MessageProcessorB() {}
};

int main(){
    MessagePassingTree<
        Edge<MessageProcessorA, MessageProcessorB, IntMessage>,
        Edge<MessageProcessorB, MessageProcessorA, DoubleMessage>> message_passing_tree;

    message_passing_tree.SendFromTo<MessageProcessorB, MessageProcessorA>(std::make_unique<DoubleMessage>(1));
    for (size_t i = 0; i < 10; ++i) {
        message_passing_tree.CallReceive(0);
        message_passing_tree.CallReceive(1);
    }
    return 0;
}
