//
// Created by ludaludaed on 02.08.2023.
//

#ifndef ATOMIC_SHARED_POINTER_MY_STACK_H
#define ATOMIC_SHARED_POINTER_MY_STACK_H

#include <optional>
#include "../src/decl_fwd.h"

namespace lu {
    template <class TValue>
    class LockFreeStack {
        struct Node {
            TValue value{};
            SharedPtr<Node> next{};
        };

    public:
        void push(const TValue &value) {
            SharedPtr<Node> new_node = makeShared<Node>();
            new_node->value = value;
            do {
                new_node->next = head_.load();
            } while (!head_.compareExchange(new_node->next, new_node));
        }

        std::optional<TValue> pop() {
            SharedPtr<Node> head = head_.load();
            if (!head) {
                return {};
            }
            while (!head_.compareExchange(head, head->next)) {
                head = head_.load();
                if (!head) {
                    return {};
                }
            }
            return {head->value};
        }

    private:
        AtomicSharedPtr<Node> head_{};
    };
}

#endif //ATOMIC_SHARED_POINTER_MY_STACK_H
