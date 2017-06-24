#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <cstdio>
#include <thread>

struct MessageBase {
public:
    virtual ~MessageBase() {}
};

template <typename TMessage>
struct WrappedMessage : MessageBase {
public:
    explicit WrappedMessage(const TMessage& content)
    : content(content)
    {}
public:
    TMessage content;
};

class Queue {
public:
    template <typename T>
    void push(const T& message) {
        std::lock_guard<std::mutex> lock(mu);
        data.push(std::make_shared<WrappedMessage<T> >(message));
        cv.notify_all();
    }
    std::shared_ptr<MessageBase> pop() {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [this] {return !data.empty();});
        auto res = data.front();
        data.pop();
        return res;
    }
private:
    std::mutex mu;
    std::condition_variable cv;
    std::queue<std::shared_ptr<MessageBase> > data;
};

class Sender {
public:
    explicit Sender(Queue* queue)
    : queue(queue)
    {}

    template<typename TMessage>
    void send(const TMessage& message) {
        if (queue != nullptr) {
            queue->push(message);
        }
    }
private:
    Queue* queue;
};

class CloseQueueMessage {};

template <typename TPreviousDispatcher, typename TMessage, typename Func>
class TemplateDispatcher {
public:
    TemplateDispatcher(const TemplateDispatcher&) = delete;
    TemplateDispatcher& operator=(const TemplateDispatcher&) = delete;

    TemplateDispatcher(TemplateDispatcher&& other)
    : queue(other.queue)
    , prev(other.prev)
    , f(std::move(other.f))
    , chained(other.chained)
    {
        other.chained = true;
    }

    TemplateDispatcher(Queue* queue, TPreviousDispatcher* prev, Func&& f)
    : queue(queue)
    , prev(prev)
    , f(std::forward<Func>(f))
    , chained(false)
    {
        prev->chained = true;
    }

    template<typename TOtherMessage, typename TOtherFunc>
    TemplateDispatcher<TemplateDispatcher, TOtherMessage, TOtherFunc> handle(TOtherFunc&& of) {
        return TemplateDispatcher<TemplateDispatcher, TOtherMessage, TOtherFunc> (queue, this, std::forward<TOtherFunc>(of));
    }

    bool dispatch(std::shared_ptr<MessageBase> message) {
        if (WrappedMessage<TMessage>* wrapper = dynamic_cast<WrappedMessage<TMessage>* >(message.get())) {
            f(wrapper->content);
            return true;
        } else {
            return prev->dispatch(message);
        }
    }

    template<typename TDispatcher, typename OtherMessage, typename OtherFunc>
    friend class TemplateDispatcher;
    void wait_and_dispatch() {
        while (true) {
            auto message = queue->pop();
            if (dispatch(message)) {
                break;
            }
        }
    }

    ~TemplateDispatcher() noexcept(false) {
        if (!chained) {
            wait_and_dispatch();
        }
    }
private:
    Queue* queue;
    TPreviousDispatcher* prev;
    Func f;
    bool chained;
};

class Dispatcher {
public:
    Dispatcher(const Dispatcher&) = delete;

    Dispatcher& operator=(const Dispatcher&) = delete;

    Dispatcher(Dispatcher&& other)
    : queue(other.queue)
    , chained(other.chained)
    {
        other.chained = true;
    }

    explicit Dispatcher(Queue* queue)
    : queue(queue)
    , chained(false)
    {}

    template<typename TMessage, typename TFunc>
    TemplateDispatcher<Dispatcher, TMessage, TFunc> handle(TFunc&& f) {
        return TemplateDispatcher<Dispatcher, TMessage, TFunc>(queue, this, std::forward<TFunc>(f));
    }

    bool dispatch(std::shared_ptr<MessageBase> message) {
        if (dynamic_cast<WrappedMessage<CloseQueueMessage> *>(message.get())) {
            throw CloseQueueMessage();
        }
        return false;
    }

    template<typename TDispatcher, typename TMessage, typename TFunc>
    friend class TemplateDispatcher;
    void wait_and_dispatch() {
        while (true) {
            auto message = queue->pop();
            dispatch(message);
        }
    }

    ~Dispatcher() noexcept(false) {
        if (!chained) {
            wait_and_dispatch();
        }
    }
private:
    Queue* queue;
    bool chained;
};

class Receiver {
public:
    Receiver(Queue* queue)
    : queue(queue)
    {}

    Dispatcher wait() {
        return Dispatcher(queue);
    }
private:
    Queue* queue;
};

class MessageChannel {
public:
    Receiver get_receiver() {
        return Receiver(&queue);
    }
    Sender get_sender() {
        return Sender(&queue);
    }
private:
    Queue queue;
};

class MessagePipeAgent {
public:
    MessagePipeAgent(Queue* input_queue, Queue *output_queue)
    : receiver(input_queue)
    , sender(output_queue)
    {}

    template<typename TMessage>
    void send(const TMessage& message) {
        sender.send(message);
    }

    Dispatcher wait() {
        return receiver.wait();
    }
private:
    Receiver receiver;
    Sender sender;
};

class MessagePipe {
public:
    MessagePipeAgent get_first_agent() {
        return MessagePipeAgent(&queue, &reverse_queue);
    }
    MessagePipeAgent get_second_agent() {
        return MessagePipeAgent(&reverse_queue, &queue);
    }
private:
    Queue queue;
    Queue reverse_queue;
};

template <typename T>
class StatefullRunner {
public:
    StatefullRunner(void (T::*initial_state)())
    : state(initial_state)
    {}

    void run() {
        try {
            while (true) {
                (((T* )this)->*state)();
            }
        } catch (const CloseQueueMessage&) {
            printf("Got CloseQueueMessage exception\n");
        }
    }

    void switch_state(void (T::*new_state)()) {
        state = new_state;
    }

    virtual ~StatefullRunner() {}
private:
    void (T::*state)();
};

// usage of message-passing //

class NumberToOutput {
public:
    explicit NumberToOutput(const int number)
    : number(number)
    {}

    int number;
};

class NumberToIncrementAndOutput {
public:
    explicit NumberToIncrementAndOutput(const int number)
    : number(number)
    {}

    int number;
};

class SwitchState {};

class IHaveSwitchedMyState {};

class Proposer : public StatefullRunner<Proposer> {
public:
    Proposer(MessagePipeAgent agent)
    : agent(agent)
    , StatefullRunner<Proposer>(&Proposer::initial_state)
    {}

    void initial_state() {
        printf("Proposer: offering number 5 to output\n");
        agent.send(NumberToOutput(5));
        printf("Proposer: I want Acceptor to switch state\n");
        agent.send(SwitchState());
        agent.wait().handle<IHaveSwitchedMyState>([&] (const IHaveSwitchedMyState&) {
            printf("Proposer: Ok fine i will switch my state too\n");
            switch_state(&Proposer::new_state);
        });
    }

    void new_state() {
        printf("Proposer: offering number 10 to increment\n");
        agent.send(NumberToIncrementAndOutput(10));
        agent.send(CloseQueueMessage());
        agent.wait();
    }
private:
    MessagePipeAgent agent;
};

class Acceptor : public StatefullRunner<Acceptor> {
public:
    Acceptor(MessagePipeAgent agent)
    : agent(agent)
    , StatefullRunner<Acceptor>(&Acceptor::initial_state)
    {}

    void initial_state() {
        agent.wait()
            .handle<NumberToOutput>([&] (const NumberToOutput& number) {
                printf("Acceptor: Got number: %d\n", number.number);
            })
            .handle<SwitchState>([&] (const SwitchState&) {
                switch_state(&Acceptor::new_state);
                printf("Acceptor: I have switched my state\n");
                agent.send(IHaveSwitchedMyState());
            });
    }

    void new_state() {
        agent.wait()
            .handle<NumberToIncrementAndOutput>([&] (const NumberToIncrementAndOutput& number) {
                printf("Acceptor: Got number to increment: %d\n", number.number + 1);
                agent.send(CloseQueueMessage());
            });
    }
private:
    MessagePipeAgent agent;
};

int main() {
    MessagePipe pipe;
    std::thread t1([&] {
        Proposer(pipe.get_first_agent()).run();
    });
    std::thread t2([&] {
        Acceptor(pipe.get_second_agent()).run();
    });
    t1.join();
    t2.join();
    return 0;
}
