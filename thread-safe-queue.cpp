#include <iostream>
#include <atomic>
#include <unordered_set>
#include <vector>
#include <thread>
#include <fstream>
#include <cstdio>

class Element {
public:
    Element()
    : next(nullptr)
    , stored_value(0)
    {}

    std::atomic<Element *> next;
    int stored_value;
};

template <typename T>
class HazardPointer {
public:
    HazardPointer(const size_t threadnum, const size_t dimention)
    : dimention(dimention)
    , threadnum(threadnum)
    , protect_count(0)
    , free_count(0)
    {
        protect_count.store(0);
        free_count.store(0);
        for (size_t i = 0; i < dimention * threadnum; ++i) {
            stored_pointers[i].store(nullptr);
        }
    }

    T * protect(const std::atomic<T *>& pointer, const size_t thread_id, const size_t num) {
        T* old_value;
        T* new_value;
        while (true) {
            protect_count.fetch_add(1);
            if (free_count.load() == 0) {
                do {
                    old_value = pointer.load();
                    stored_pointers[thread_id * dimention + num].store(old_value);
                    new_value = pointer.load();
                } while (old_value != new_value);
                protect_count.fetch_add(-1);
                break;
            } else {
            }
            protect_count.fetch_add(-1);
            sched_yield();
        }
        return old_value;
    }

    void remove(T * pointer) {
        pointers_to_delete.insert(pointer);
        real_remove();
    }

    void real_remove() {
        free_count.fetch_add(1);
        while (protect_count.load() > 0) {
            sched_yield();
        }
        std::unordered_set<T *> registered_pointers;
        for (size_t i = 0 ; i < dimention * threadnum; ++i ){
            T* pointer = stored_pointers[i].load();
            if (pointer != nullptr) {
                registered_pointers.insert(pointer);
            }
        }
        auto old_pointers_to_delete = pointers_to_delete;
        for (auto& pointer: old_pointers_to_delete) {
            if (registered_pointers.count(pointer) == 0) {
                delete pointer;
                pointers_to_delete.erase(pointer);
            }
        }
        free_count.fetch_add(-1);
    }

    void cleanup(const size_t thread_id) {
        for (size_t i = 0; i < dimention; ++i) {
            stored_pointers[thread_id * dimention + i].store(nullptr);
        }
        real_remove();
    }

    ~HazardPointer() {
    }
private:
    size_t dimention;
    size_t threadnum;
    std::atomic<T *> stored_pointers[1000];
    thread_local static std::unordered_set<T *> pointers_to_delete;
    std::atomic<int> protect_count;
    std::atomic<int> free_count;
};

template <typename T>
thread_local std::unordered_set<T *> HazardPointer<T>::pointers_to_delete;

class ThreadSafeQueue {
public:
    ThreadSafeQueue() {
        Element * new_element = new Element();
        head.store(new_element);
        tail.store(new_element);
    }

    void enqueue(const int value, const size_t thread_id) {
        printf("Started ENC %d\n", value);
        Element * new_element = new Element();
        new_element->stored_value = value;
        Element * t;
        while (true) {
            t = hazard_pointer.protect(tail, thread_id, 0);
            Element* t_next = t->next.load();
            if (t_next != nullptr) {
                tail.compare_exchange_weak(t, t_next);
                continue;
            }
            Element* empty = nullptr;
            if (t->next.compare_exchange_weak(empty, new_element)) {
                fprintf(stdout, "Finished ENC %d\n", value);
                break;
            }
        }
        tail.compare_exchange_weak(t, new_element);
    }
    int dequeue(const size_t thread_id) {
        printf("Started DEQ\n");
        int number = 0;
        while (true) {
            auto h = hazard_pointer.protect(head, thread_id, 1);
            auto h_next = hazard_pointer.protect(h->next, thread_id, 2);

            if (h != head.load()) {
                continue;
            }

            if (h_next == nullptr) {
                printf("Finished DEQ: EMPTY\n");
                return -1;
            }
            if (h == tail.load()) {
                tail.compare_exchange_weak(h, h_next);
                continue;
            }
            if (head.compare_exchange_weak(h, h_next)) {
                hazard_pointer.remove(h);
                int result_value = h_next->stored_value;
                printf("Finished DEQ %d\n", result_value);
                return result_value;
            }
        }
    }

    void cleanup_locals(const size_t thread_id) {
        hazard_pointer.cleanup(thread_id);
    }

    ~ThreadSafeQueue() {
        Element* to_delete = head.load();
        while (to_delete) {
            Element * next_pointer = to_delete->next;
            delete to_delete;
            to_delete = next_pointer;
        }
    }
private:
    std::atomic<Element *> head;
    std::atomic<Element *> tail;
    static HazardPointer<Element> hazard_pointer;
};

HazardPointer<Element> ThreadSafeQueue::hazard_pointer(4, 3);

int main() {
    ThreadSafeQueue queue;
    std::thread first([&] () {
        queue.enqueue(1, 0);
        queue.enqueue(2, 0);
        queue.enqueue(3, 0);
        queue.cleanup_locals(0);
    });
    std::thread second([&] () {
        queue.dequeue(1);
        queue.dequeue(1);
        queue.dequeue(1);
        queue.dequeue(1);
        queue.cleanup_locals(1);
    });
    std::thread third([&] () {
        queue.enqueue(4, 2);
        queue.enqueue(5, 2);
        queue.enqueue(6, 2);
        queue.cleanup_locals(2);
    });
    std::thread fourth([&] () {
        queue.dequeue(3);
        queue.dequeue(3);
        queue.cleanup_locals(3);
    });
    fourth.join();
    second.join();
    first.join();
    third.join();
    return 0;
}
