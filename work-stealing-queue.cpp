#include <deque>
#include <memory>
#include <mutex>

class FunctionWrapper {
private:
    struct ImplBase {
        virtual void Call() = 0;
        virtual ~ImplBase() {}
    };
    template <typename F>
    struct ImplType : ImplBase {
        ImplType(F&& f)
        : f(std::move(f))
        {}
        void Call() {
            f();
        }
        F f;
    };
public:
    template <typename F>
    FunctionWrapper(F&& f)
    : function_impl(new ImplType<F>(std::move(f)))
    {}
    void operator()() {
        function_impl->Call();
    }
    FunctionWrapper() = delete;

    FunctionWrapper(FunctionWrapper&& other)
    : function_impl(std::move(other.function_impl))
    {}
    FunctionWrapper& operator=(FunctionWrapper&& other) {
        function_impl = std::move(other.function_impl);
        return *this;
    }
    FunctionWrapper(const FunctionWrapper&) = delete;
    FunctionWrapper(FunctionWrapper&) = delete;
    FunctionWrapper& operator=(const FunctionWrapper&) = delete;
private:
    std::unique_ptr<ImplBase> function_impl;
};

class WorkStealingQueue {
private:
    typedef FunctionWrapper TData;
public:
    WorkStealingQueue()
    {}

    WorkStealingQueue(const WorkStealingQueue& other) = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue& other) = delete;

    void Push(TData data) {
        std::lock_guard<std::mutex> cur_lock(mu);
        data_queue.push_front(std::move(data));
    }
    bool Empty() const {
        std::lock_guard<std::mutex> cur_lock(mu);
        return data_queue.empty();
    }
    bool TryPop(TData& res) {
        std::lock_guard<std::mutex> cur_lock(mu);
        if (data_queue.empty()) {
            return false;
        }
        res = std::move(data_queue.front());
        data_queue.pop_front();
        return true;
    }
    bool TrySteal(TData& res) {
        std::lock_guard<std::mutex> cur_lock(mu);
        if (data_queue.empty()) {
            return false;
        }
        res = std::move(data_queue.back());
        data_queue.pop_back();
        return true;
    }
private:
    std::deque<TData> data_queue;
    mutable std::mutex mu;
};
int main() {
    return 0;
}
