#include <list>
#include <future>
#include <iostream>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <algorithm>

template <typename T>
class LockFreeStack {
public:
    LockFreeStack() {
    }
    void push(T&& data) {
        CountedNodePtr new_node;
        new_node.ptr = new Node(std::move(data));
        new_node.external_count = 1;
        new_node.ptr->next = head.load(std::memory_order_relaxed);
        while (!head.compare_exchange_weak(new_node.ptr->next, new_node,
                    std::memory_order_release, std::memory_order_relaxed)) {}
    }
    void push(const T& data) {
        CountedNodePtr new_node;
        new_node.ptr = new Node(data);
        new_node.external_count = 1;
        new_node.ptr->next = head.load(std::memory_order_relaxed);
        while (!head.compare_exchange_weak(new_node.ptr->next, new_node,
                    std::memory_order_release, std::memory_order_relaxed)) {}
    }
    std::shared_ptr<T> pop() {
        CountedNodePtr old_head = head.load(std::memory_order_relaxed);
        while (true) {
            increase_head_count(old_head);
            Node* ptr = old_head.ptr;
            if (!ptr) {
                return std::shared_ptr<T>();
            }
            if (head.compare_exchange_strong(old_head, ptr->next, std::memory_order_relaxed, std::memory_order_relaxed)) {
                std::shared_ptr<T> res;
                res.swap(ptr->data);
                int count_increase = old_head.external_count - 2;
                if (ptr->internal_count.fetch_add(count_increase, std::memory_order_release) == -count_increase) {
                    delete ptr;
                }
                return res;
            } else if (ptr->internal_count.fetch_sub(1, std::memory_order_relaxed) == 1) {
                ptr->internal_count.load(std::memory_order_acquire);
                delete ptr;
            }
        }
    }
    ~LockFreeStack() {
        while(pop()) {}
    }
private:
    struct Node;
    struct CountedNodePtr {
        int external_count = 0;
        Node* ptr = nullptr;
    };
    struct Node {
        std::shared_ptr<T> data;
        std::atomic<int> internal_count;
        CountedNodePtr next;

        Node(T&& data)
        : data(std::make_shared<T>(std::move(data)))
        , internal_count(0)
        {}

        Node(const T& data)
        : data(std::make_shared<T>(data))
        , internal_count(0)
        {}
    };
    void increase_head_count(CountedNodePtr& old_counter) {
        CountedNodePtr new_counter;
        do {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while (!head.compare_exchange_strong(old_counter, new_counter, std::memory_order_acquire, std::memory_order_relaxed));
        old_counter.external_count = new_counter.external_count;
    }
    std::atomic<CountedNodePtr> head;
};

template <typename T>
struct Sorter {
    struct ChunkToSort {
        std::list<T> data;
        std::promise<std::list<T> > promise;
    };

    Sorter()
    : max_thread_count(std::thread::hardware_concurrency() - 1)
    , end_of_data(false)
    {}

    ~Sorter() {
        end_of_data = true;
        for (size_t i = 0; i < threads.size(); ++i) {
            threads[i].join();
        }
    }

    void try_sort_chunk() {
        std::shared_ptr<ChunkToSort> chunk = chunks.pop();
        if (chunk) {
            sort_chunk(chunk);
        }
    }

    std::list<T> do_sort(std::list<T>& chunk_data) {
        if (chunk_data.empty()) {
            return chunk_data;
        }

        std::list<T> result;
        result.splice(result.begin(), chunk_data, chunk_data.begin());
        const T& partition_val = *result.begin();
        typename std::list<T>::iterator divide_point = std::partition(chunk_data.begin(),
                chunk_data.end(), [&](const T& val) {return val < partition_val;});
        ChunkToSort new_lower_chunk;
        new_lower_chunk.data.splice(new_lower_chunk.data.end(), chunk_data, chunk_data.begin(), divide_point);
        std::future<std::list<T>> new_lower = new_lower_chunk.promise.get_future();
        chunks.push(std::move(new_lower_chunk));
        if (threads.size() < max_thread_count) {
            std::cout << "Swaping thread\n";
            threads.push_back(std::thread(&Sorter<T>::sort_thread, this));
        }
        std::list<T> new_higher(do_sort(chunk_data));
        result.splice(result.end(), new_higher);
        while (new_lower.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            try_sort_chunk();
        }
        result.splice(result.begin(), new_lower.get());
        return result;
    }

    void sort_chunk(const std::shared_ptr<ChunkToSort>& chunk) {
        chunk->promise.set_value(do_sort(chunk->data));
    }

    void sort_thread() {
        while (!end_of_data) {
            try_sort_chunk();
            std::this_thread::yield();
        }
    }

    LockFreeStack<ChunkToSort> chunks;
    std::vector<std::thread> threads;
    unsigned const max_thread_count;
    std::atomic<bool> end_of_data;
};

template <typename T>
std::list<T> parallel_quick_sort(std::list<T> input) {
    if (input.empty()) {
        return input;
    }
    Sorter<T> s;
    return s.do_sort(input);
}

int main() {
    std::list<int> list{11,2,13,4,0,-1,2};
    list = parallel_quick_sort(list);
    for (auto& el: list) {std::cout << " " << el;}
    std::cout << std::endl;
    return 0;
}
