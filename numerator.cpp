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

    template <typename GlobalPiper>
    class MessageProcessorProxy {
    public:
        MessageProcessorProxy(GlobalPiper& piper, const bool is_to)
        : piper(piper)
        , is_to(is_to)
        {}

        virtual void Ping() const = 0;
        virtual ~MessageProcessorProxy() {}
    protected:
        GlobalPiper& piper;
        bool is_to;
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
    void FillEdgeProxysImpl(GlobalPiper&, std::vector<std::unique_ptr<Piper::EdgeProxy<GlobalPiper>>>&) {}
    void GetAssosiatedEdgesImpl() {}
    void GetEdgeIndexImpl() {}
    template <typename GlobalPiper>
    void AddMessageProcessorsImpl(GlobalPiper& ,
                                  std::vector<std::unique_ptr<Piper::MessageProcessorProxy<GlobalPiper>>>&) {}

    virtual ~Piper() {}
};

template <typename T>
class ReceivingFrom {};

template <typename From, typename To, typename Message, typename ... Args>
class Piper<Edge<From, To, Message>, Args...> : public Piper<Args...> {
protected:
    template <typename GlobalPiper, typename From2>
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

    template <typename GlobalPiper>
    class MessageProcessorProxy : public Piper<Args...>::template MessageProcessorProxy<GlobalPiper> {
    public:
        using Piper<Args...>::template MessageProcessorProxy<GlobalPiper>::MessageProcessorProxy;
        using Piper<Args...>::template MessageProcessorProxy<GlobalPiper>::piper;
        using Piper<Args...>::template MessageProcessorProxy<GlobalPiper>::is_to;

        template <typename MP>
        void PingImpl(MessageProcessorBase* message_processor) const {
            MP& cur_message_processor = *dynamic_cast<MP*>(message_processor);
            cur_message_processor.Ping(SenderProxy<GlobalPiper, MP>(piper));
        }

        virtual void Ping() const {
            auto& cur_piper = *dynamic_cast<Piper *>(&piper);
            int message_processor_index;
            if (is_to) {
                message_processor_index = cur_piper.cur_to_index;
            } else {
                message_processor_index = cur_piper.cur_from_index;
            }
            MessageProcessorBase* message_processor = cur_piper.message_processors[message_processor_index].get();
            if (is_to) {
                PingImpl<To>(message_processor);
            } else {
                PingImpl<From>(message_processor);
            }
        }
    };

    template <typename GlobalPiper>
    class EdgeProxy : public Piper<Args...>::template EdgeProxy<GlobalPiper> {
    public:
        using Piper<Args...>::template EdgeProxy<GlobalPiper>::EdgeProxy;
        using Piper<Args...>::template EdgeProxy<GlobalPiper>::piper;

        virtual void NotifyAboutMessage() const {
            auto& cur_piper = *dynamic_cast<Piper *>(&piper);
            To* message_processor = dynamic_cast<To*>(cur_piper.message_processors[cur_piper.cur_to_index].get());
            auto& current_queue = cur_piper.queues[cur_piper.cur_edge_index];
            Message* value = dynamic_cast<Message*>(current_queue.front().get());
            current_queue.pop_front();
            message_processor->Receive(ReceivingFrom<From>(), *value, SenderProxy<GlobalPiper, To>(piper));
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
    }

    template <typename GlobalPiper, typename MP>
    void AddMessageProcessorIfNotExists(int& target_index,
                                        GlobalPiper& piper,
                                        std::vector<std::unique_ptr<Piper<>::MessageProcessorProxy<GlobalPiper>>>& message_processor_handlers,
                                        const bool is_to) {
        target_index = Piper<Args...>::GetMessageProcessorIndexImpl(TypeSpecifier<MP>());
        if (target_index == -1) {
            target_index = max_message_processor_index++;
            dest_pipes.push_back(std::vector<int>());
            message_processors.push_back(std::make_unique<MP>());
            message_processor_handlers.push_back(std::make_unique<MessageProcessorProxy<GlobalPiper>>(piper, is_to));
        }
    }

    template <typename GlobalPiper>
    void AddMessageProcessorsImpl(GlobalPiper& piper,
                                  std::vector<std::unique_ptr<Piper<>::MessageProcessorProxy<GlobalPiper>>>& message_processor_handlers) {
        Piper<Args...>::template AddMessageProcessorsImpl<GlobalPiper>(piper, message_processor_handlers);
        AddMessageProcessorIfNotExists<GlobalPiper, From>(cur_from_index, piper, message_processor_handlers, false);
        AddMessageProcessorIfNotExists<GlobalPiper, To>(cur_to_index, piper, message_processor_handlers, true);
        cur_edge_index = max_edge_index++;
        dest_pipes[cur_to_index].push_back(cur_edge_index);
        queues = std::vector<std::deque<std::unique_ptr<MessageBase>>>(max_edge_index);
    }

    template <typename GlobalPiper>
    void FillEdgeProxysImpl(GlobalPiper& piper, std::vector<std::unique_ptr<Piper<>::EdgeProxy<GlobalPiper>>>& edge_handers) {
        Piper<Args...>::template FillEdgeProxysImpl<GlobalPiper>(piper, edge_handers);
        edge_handers.push_back(std::make_unique<EdgeProxy<GlobalPiper>>(piper));
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

    virtual ~Piper() {}
private:
    int cur_to_index;
    int cur_from_index;
    int cur_edge_index;
};

template <typename ... Args>
class MessagePassingTree : public Piper<Args...> {
private:
    using GlobalPiper = Piper<Args...>;
public:
    MessagePassingTree()
    : GlobalPiper()
    {
        GlobalPiper::template AddMessageProcessorsImpl<GlobalPiper>(*this, message_processor_handlers);
        GlobalPiper::template FillEdgeProxysImpl<GlobalPiper>(*this, edge_handers);
    }

    const Piper<>::EdgeProxy<GlobalPiper>* GetEdgeProxy(const int index) {
        return edge_handers[index].get();
    }

    const Piper<>::MessageProcessorProxy<GlobalPiper>* GetMessageProcessorProxy(const int index) {
        return message_processor_handlers[index].get();
    }
private:
    std::vector<std::unique_ptr<Piper<>::EdgeProxy<GlobalPiper>>> edge_handers;
    std::vector<std::unique_ptr<Piper<>::MessageProcessorProxy<GlobalPiper>>> message_processor_handlers;
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
    void Ping(const Sender& sender) {
        std::cout << "Called Ping to MessageProcessorB\n";
        sender.template Send<MessageProcessorA>(std::make_unique<DoubleMessage>(1));
    }

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

    message_passing_tree.GetMessageProcessorProxy(0)->Ping();
    for (size_t i = 0; i < 10; ++i) {
        message_passing_tree.GetEdgeProxy(0)->NotifyAboutMessage();
        message_passing_tree.GetEdgeProxy(1)->NotifyAboutMessage();
    }
    return 0;
}
