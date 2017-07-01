#include <iostream>
#include <thread>
#include <stdexcept>
#include <atomic>
#include <memory>

const size_t MAX_HAZARD_POINTERS = 100;
struct HazardPointer {
    std::atomic<std::thread::id> id;
    std::atomic<void*> pointer;
};
HazardPointer harazd_pointers[MAX_HAZARD_POINTERS];
class HPOwner {
public:
    HPOwner(const HPOwner&) = delete;
    HPOwner& operator=(const HPOwner&) = delete;
    HPOwner()
    : hp(nullptr)
    {
        for (size_t i = 0; i < MAX_HAZARD_POINTERS; ++i) {
            std::thread::id old_id;
            if (harazd_pointers[i].id.compare_exchange_strong(old_id,
                    std::this_thread::get_id())) {
                hp = &harazd_pointers[i];
                break;
            }
        }
        if (!hp) {
            throw std::runtime_error("No hp available");
        }
    }

    std::atomic<void*>& get_pointer() {
        return hp->pointer;
    }
    ~HPOwner() {
        hp->pointer.store(nullptr);
        hp->id.store(std::thread::id());
    }
private:
    HazardPointer* hp;
};

std::atomic<void *>& get_hazard_pointer_for_current_thread() {
    thread_local static HPOwner hazard;
    return hazard.get_pointer();
}

bool outstanding_hazard_pointers_for(void* p) {
    for (size_t i = 0; i < MAX_HAZARD_POINTERS; ++i) {
        if (harazd_pointers[i].pointer.load() == p) {
            return true;
        }
    }
    return false;
}

template <typename T>
void do_delete(void * p) {
    delete static_cast<T*>(p);
}

struct DataToReclaim {
    template <typename T>
    DataToReclaim(T* p)
    : data(p)
    , deleter(&do_delete<T>)
    , next(nullptr)
    {}

    ~DataToReclaim() {
        deleter(data);
    }

    void* data;
    std::function<void(void*)> deleter;
    DataToReclaim* next;
};

std::atomic<DataToReclaim*> nodes_to_reclaim;

void add_to_reclaim_list(DataToReclaim* node) {
    node->next = nodes_to_reclaim.load();
    while (!nodes_to_reclaim.compare_exchange_weak(node->next, node)) {}
}

template<typename T>
void reclaim_later(T* data) {
    add_to_reclaim_list(new DataToReclaim(data));
}

void delete_nodes_with_no_hazards() {
    DataToReclaim* current = nodes_to_reclaim.exchange(nullptr);
    while (current) {
        DataToReclaim* next = current->next;
        if (!outstanding_hazard_pointers_for(current->data)) {
            delete current;
        } else {
            add_to_reclaim_list(current);
        }
        current = next;
    }
}

template <typename T>
class LockFreeStack {
public:
    LockFreeStack()
    : head(nullptr)
    , to_be_deleted(nullptr)
    {}

    void push(const T& data) {
        Node* new_node = new Node(data);
        new_node->next = head.load();
        while (!head.compare_exchange_weak(new_node->next, new_node)) {}
    }
    std::shared_ptr<T> pop() {
        std::atomic<void*>& hp = get_hazard_pointer_for_current_thread();
        Node* old_head = head.load();
        do {
            Node* temp;
            do {
                temp = old_head;
                hp.store(old_head);
                old_head = head.load();
            } while (temp != old_head);
        } while (old_head && !head.compare_exchange_strong(old_head, old_head->next));
        hp.store(nullptr);
        std::shared_ptr<T> res;
        if (old_head) {
            res.swap(old_head->data);
            if (outstanding_hazard_pointers_for(old_head)) {
                reclaim_later(old_head);
            } else {
                delete old_head;
            }
            delete_nodes_with_no_hazards();
        }
        return res;
    }
private:
    struct Node {
        std::shared_ptr<T> data;
        Node* next;

        Node(const T& data)
        : data(std::make_shared<T>(data))
        {}
    };

    std::atomic<Node*> to_be_deleted = nullptr;
    std::atomic<Node*> head;
    std::atomic<int> threads_in_pop;
};
int main() {
    LockFreeStack<int> stack;
    stack.push(5);
    std::cout << *stack.pop() << std::endl;
    return 0;
}
