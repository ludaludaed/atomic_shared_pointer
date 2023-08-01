#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include "src/fwd.h"
#include "src/hazard_pointer_domain.h"
#include "src/atomic_shared_pointer.h"
#include "src/thread_entry_list.h"


using namespace lu;

template <class TValue>
class LockFreeStack {
    struct Node {
        TValue value{};
        SharedPtr<Node> next{};
    };

public:
    void push(const TValue &value) {
        SharedPtr<Node> new_node = makeShared<Node>();
//        SharedPtr<Node> new_node(new Node, [](Node *node) {
//            std::cout << node->value << "!" << std::endl;
//            delete node;
//        });
        new_node->value = value;
        do {
            new_node->next = head_.load();
        } while (!head_.compareExchange(new_node->next, new_node));
    }

    TValue pop() {
        SharedPtr<Node> head = head_.load();
        while (!head_.compareExchange(head, head->next));
        return head->value;
    }

private:
    AtomicSharedPtrHP<Node> head_;
};

int main() {
    LockFreeStack<int> stack;

    std::vector<std::thread> workers;

    for (int i = 0; i < 100; ++i) {
        workers.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j) {
                stack.push(j);
                std::cout << stack.pop() << std::endl;
            }
        });
    }

    for (int i = 0; i < 100; ++i) {
        workers[i].join();
    }



//    HazardPointerDomain<> &inst = HazardPointerDomain<>::instance();
//    int *ptr = new int(10);
//    std::atomic<int *> atom{ptr};
//    auto first = inst.protect(atom);
//    inst.retire<Dispose>(ptr);
    return 0;
}
