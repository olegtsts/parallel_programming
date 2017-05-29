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
//            fprintf(stderr, "Starting protect, thread %d\n", std::this_thread::get_id());
            if (free_count.load() == 0) {
//                fprintf(stderr, "Inside free count check\n");
                do {
                    old_value = pointer.load();
                    stored_pointers[thread_id * dimention + num].store(old_value);
                    new_value = pointer.load();
//                    fprintf(stderr, "Processing protect: %p against %p\n", (void *)old_value, (void *)new_value);
//                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } while (old_value != new_value);
                protect_count.fetch_add(-1);
                break;
            } else {
//                fprintf(stderr, "Outside free count check, thread %d, value %d\n", std::this_thread::get_id(), free_count.load());
            }
            protect_count.fetch_add(-1);
//            fprintf(stderr, "Done protect\n");
//            std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
//        fprintf(stderr, "Waiting for protects to finish, thread %d\n", std::this_thread::get_id());
        while (protect_count.load() > 0) {
//            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//            fprintf(stderr, "Projects count: %d\n", protect_count.load());
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
//                printf("Freeing pointer %p, thread %d\n", (void *)pointer, std::this_thread::get_id());
                delete pointer;
                pointers_to_delete.erase(pointer);
            }
        }
        free_count.fetch_add(-1);
//        fprintf(stderr, "Done finishing, thread %d\n", std::this_thread::get_id());
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
//            fprintf(stderr, "thread_id %lu: before protecting tail\n", thread_id);
            t = hazard_pointer.protect(tail, thread_id, 0);
//            fprintf(stderr, "thread_id %lu: done protecting tail %p\n", thread_id, (void *)t);
            Element* t_next = t->next.load();
            if (t_next != nullptr) {
                tail.compare_exchange_weak(t, t_next);
//                fprintf(stderr, "thread_id %lu: bad tail: move it from %p to %p\n", thread_id, (void *)t, (void *)t_next);
                continue;
            }
            Element* empty = nullptr;
            if (t->next.compare_exchange_weak(empty, new_element)) {
//                fprintf(stderr, "thread_id %lu: Done pointing to new element\n", thread_id);
                fprintf(stdout, "Finished ENC %d\n", value);
                break;
            }
        }
//        fprintf(stderr, "thread_id %lu: exit cycle of enqueue\n", thread_id);
        tail.compare_exchange_weak(t, new_element);
    }
    int dequeue(const size_t thread_id) {
        printf("Started DEQ\n");
        int number = 0;
        while (true) {
//            fprintf(stderr, "thread_id %lu: before protecting head\n", thread_id);
            auto h = hazard_pointer.protect(head, thread_id, 1);
//            fprintf(stderr, "thread_id %lu: done protecting head %p\n", thread_id, (void *)h);
            auto h_next = hazard_pointer.protect(h->next, thread_id, 2);
//            fprintf(stderr, "thread_id %lu: done protecting head next %p\n", thread_id, (void *)h_next);

            if (h != head.load()) {
                continue;
            }

            if (h_next == nullptr) {
//                fprintf(stderr, "thread_id %lu: null caught\n", thread_id);
                printf("Finished DEQ: EMPTY\n");
                return -1;
            }
            if (h == tail.load()) {
                tail.compare_exchange_weak(h, h_next);
//                fprintf(stderr, "thread_id %lu: bad tail: move it 2\n", thread_id);
                continue;
            }
            if (head.compare_exchange_weak(h, h_next)) {
//                fprintf(stderr, "thread_id %lu: Before hazard remove\n", thread_id);
                hazard_pointer.remove(h);
//                fprintf(stderr, "thread_id %lu: Done mark removing, done pop\n", thread_id);
                int result_value = h_next->stored_value;
                printf("Finished DEQ %d\n", result_value);
                return result_value;
            }
        }
//        fprintf(stderr, "thread_id %lu: exit cycle of dequeue\n", thread_id);
    }

    void cleanup_locals(const size_t thread_id) {
        hazard_pointer.cleanup(thread_id);
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
//        printf("Thread id 0: %d\n", std::this_thread::get_id());
        queue.enqueue(1, 0);
        queue.enqueue(2, 0);
        queue.enqueue(3, 0);
        queue.cleanup_locals(0);
//        printf("Thread id 0 done: %d\n", std::this_thread::get_id());
    });
    std::thread second([&] () {
//        printf("Thread id 1: %d\n", std::this_thread::get_id());
        queue.dequeue(1);
        queue.dequeue(1);
        queue.dequeue(1);
        queue.dequeue(1);
        queue.cleanup_locals(1);
//        printf("Thread id 1 done: %d\n", std::this_thread::get_id());
    });
    std::thread third([&] () {
//        printf("Thread id 2: %d\n", std::this_thread::get_id());
        queue.enqueue(4, 2);
        queue.enqueue(5, 2);
        queue.enqueue(6, 2);
        queue.cleanup_locals(2);
//        printf("Thread id 2 done: %d\n", std::this_thread::get_id());
    });
    std::thread fourth([&] () {
//        printf("Thread id 3: %d\n", std::this_thread::get_id());
        queue.dequeue(3);
        queue.dequeue(3);
        queue.cleanup_locals(3);
//        printf("Thread id 3 done: %d\n", std::this_thread::get_id());
    });
    fourth.join();
    second.join();
    first.join();
    third.join();
    return 0;
}
