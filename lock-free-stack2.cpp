#include <iostream>
#include <atomic>
#include <memory>

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
        ++threads_in_pop;
        Node* old_head = head.load();
        while (old_head && !head.compare_exchange_weak(old_head, old_head->next)) {}
        std::shared_ptr<T> res;
        if (old_head) {
            res.swap(old_head->data);
        }
        try_reclaim(old_head);
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
    static void delete_nodes(Node* nodes) {
        while (nodes) {
            Node* next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }

    void chain_pending_nodes(Node* nodes) {
        Node* last = nodes;
        while (Node* next = last->next) {
            last = next;
        }
        chain_pending_nodes(nodes,last);
    }

    void chain_pending_nodes(Node* first, Node* last) {
        last->next = to_be_deleted;
        while (!to_be_deleted.compare_exchange_weak(last->next, first)) {}
    }

    void chain_pending_node(Node* n) {
        chain_pending_nodes(n, n);
    }

    void try_reclaim(Node* old_head) {
        if (threads_in_pop == 1) {
            Node* nodes_to_delete = to_be_deleted.exchange(nullptr);
            if (!--threads_in_pop) {
                delete_nodes(nodes_to_delete);
            } else if (nodes_to_delete) {
                chain_pending_nodes(nodes_to_delete);
            }
            delete old_head;
        } else {
            chain_pending_node(old_head);
            --threads_in_pop;
        }
    }
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
