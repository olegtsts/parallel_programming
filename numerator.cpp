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
    template <typename Sender>
    void Ping(const Sender&) {}

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
    class EdgeProxy {
    public:
        EdgeProxy(GlobalPiper& piper)
        : piper(piper)
        {}

        virtual void NotifyAboutMessage() const = 0;
        virtual ~EdgeProxy() {}
    protected:
        GlobalPiper& piper;
    };
    int max_message_processor_index;
    int max_edge_index;
    std::vector<std::vector<int>> dest_pipes;
    std::vector<std::deque<std::unique_ptr<MessageBase>>> queues;
    std::vector<std::unique_ptr<MessageProcessorBase>> message_processors;
public:
    Piper()
    : max_message_processor_index(0)
    , max_edge_index(0)
    {
    }

    template <typename MP>
    int GetMessageProcessorIndexImpl(const TypeSpecifier<MP>&) {
        return -1;
    }

    template <typename GlobalPiper>
    void FillEdgeProxysImpl(std::vector<std::unique_ptr<Piper::EdgeProxy<GlobalPiper>>>&, GlobalPiper&) {}
    void GetAssosiatedEdgesImpl() {}
    void GetEdgeIndexImpl() {}

    virtual ~Piper() {}
};

template <typename T>
class ReceivingFrom {};

template <typename From, typename To, typename Message, typename ... Args>
class Piper<Edge<From, To, Message>, Args...> : public Piper<Args...> {
protected:
    template <typename GlobalPiper>
    class EdgeProxy : public Piper<Args...>::template EdgeProxy<GlobalPiper> {
    private:
        template <typename From2>
        class SenderProxy {
        public:
            SenderProxy(GlobalPiper& piper)
            : piper(piper)
            {}

            template <typename To2, typename Message2>
            void Send(std::unique_ptr<Message2> message) const {
                int edge_index = piper.GetEdgeIndexImpl(TypeSpecifier<Edge<From2, To2, Message2>>());
                piper.queues[edge_index].push_back(std::move(message));
            }
        private:
            GlobalPiper& piper;
        };
    public:
        using Piper<Args...>::template EdgeProxy<GlobalPiper>::EdgeProxy;
        using Piper<Args...>::template EdgeProxy<GlobalPiper>::piper;

        virtual void NotifyAboutMessage() const {
            auto& cur_piper = *dynamic_cast<Piper *>(&piper);
            To* message_processor = dynamic_cast<To*>(cur_piper.message_processors[cur_piper.cur_to_index].get());
            auto& current_queue = cur_piper.queues[cur_piper.cur_edge_index];
            Message* value = dynamic_cast<Message*>(current_queue.front().get());
            current_queue.pop_front();
            message_processor->Receive(ReceivingFrom<From>(), *value, SenderProxy<To>(piper));
        }
        virtual ~EdgeProxy() {}
    };
public:
    using Piper<Args...>::max_message_processor_index;
    using Piper<Args...>::GetMessageProcessorIndexImpl;
    using Piper<Args...>::max_edge_index;
    using Piper<Args...>::dest_pipes;
    using Piper<Args...>::GetAssosiatedEdgesImpl;
    using Piper<Args...>::queues;
    using Piper<Args...>::message_processors;
    using Piper<Args...>::GetEdgeIndexImpl;
    Piper()
    : Piper<Args...>()
    {
        AddMessageProcessorIfNotExists<From>(cur_from_index);
        AddMessageProcessorIfNotExists<To>(cur_to_index);
        cur_edge_index = max_edge_index++;
        dest_pipes[cur_to_index].push_back(cur_edge_index);
    }

    template <typename MP>
    void AddMessageProcessorIfNotExists(int& target_index) {
        target_index = Piper<Args...>::GetMessageProcessorIndexImpl(TypeSpecifier<MP>());
        if (target_index == -1) {
            target_index = max_message_processor_index++;
            dest_pipes.push_back(std::vector<int>());
            message_processors.push_back(std::make_unique<MP>());
        }
    }

    template <typename GlobalPiper>
    void FillEdgeProxysImpl(std::vector<std::unique_ptr<Piper<>::EdgeProxy<GlobalPiper>>>& handlers, GlobalPiper& piper) {
        Piper<Args...>::template FillEdgeProxysImpl<GlobalPiper>(handlers, piper);
        handlers.push_back(std::make_unique<EdgeProxy<GlobalPiper>>(piper));
    }

    void LastLevelInit() {
        FillEdgeProxysImpl<Piper>(edge_handers, *this);
        queues = std::vector<std::deque<std::unique_ptr<MessageBase>>>(max_edge_index);
    }

    int GetMessageProcessorIndexImpl(const TypeSpecifier<From>&) {
        return cur_from_index;
    }
    int GetMessageProcessorIndexImpl(const TypeSpecifier<To>&) {
        return cur_to_index;
    }
    int GetEdgeIndexImpl(const TypeSpecifier<Edge<From, To, Message>>&) {
        return cur_edge_index;
    }

    std::vector<int> GetAssosiatedEdgesImpl(const TypeSpecifier<To>& specifier) {
        return dest_pipes[GetMessageProcessorIndexImpl(specifier)];
    }

    template <typename From2, typename To2, typename Message2>
    void PushMessageToQueue(std::unique_ptr<Message2> message) {
        int edge_index = GetEdgeIndexImpl(TypeSpecifier<Edge<From2, To2, Message2>>());
        queues[edge_index].push_back(std::move(message));
    }

    const Piper<>::EdgeProxy<Piper>* GetEdgeProxy(const int index) {
        return edge_handers[index].get();
    }

    virtual ~Piper() {}
private:
    int cur_to_index;
    int cur_from_index;
    int cur_edge_index;
    std::vector<std::unique_ptr<Piper<>::EdgeProxy<Piper>>> edge_handers;
};

template <typename ... Args>
class MessagePassingTree : public Piper<Args...> {
public:
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
    void Receive(const ReceivingFrom<MessageProcessorB>&, const DoubleMessage& value, const Sender& sender) {
        std::cout << "MessageProcessorA: I have got int value from MessageProcessorB " << value.a << std::endl;
        sender.template Send<MessageProcessorB>(std::make_unique<IntMessage>(2 + static_cast<int>(value.a)));
    }
    virtual ~MessageProcessorA(){}
};

class MessageProcessorB : public MessageProcessorBase {
public:
    using MessageProcessorBase::MessageProcessorBase;

    template <typename Sender>
    void Receive(const ReceivingFrom<MessageProcessorA>&, const IntMessage& value, const Sender& sender) {
        std::cout << "MessageProcessorB: I have got int value from MessageProcessorA " << value.a << std::endl;
        sender.template Send<MessageProcessorA>(std::make_unique<DoubleMessage>(3 + value.a));
    }
    template <typename Sender>
    void Receive(const ReceivingFrom<MessageProcessorA>&, const DoubleMessage& value, const Sender&) {
        std::cout << "MessageProcessorB: I have got double value from MessageProcessorA"  << value.a << std::endl;
    }
    virtual ~MessageProcessorB() {}
};

int main(){
    MessagePassingTree<
        Edge<MessageProcessorA, MessageProcessorB, IntMessage>,
        Edge<MessageProcessorB, MessageProcessorA, DoubleMessage>> message_passing_tree;

    message_passing_tree.PushMessageToQueue<MessageProcessorB, MessageProcessorA>(std::make_unique<DoubleMessage>(1));
    for (size_t i = 0; i < 10; ++i) {
        message_passing_tree.GetEdgeProxy(0)->NotifyAboutMessage();
        message_passing_tree.GetEdgeProxy(1)->NotifyAboutMessage();
    }
    return 0;
}
