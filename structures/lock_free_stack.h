//
// Created by ludaludaed on 02.08.2023.
//

#ifndef ATOMIC_SHARED_POINTER_LOCK_FREE_STACK_H
#define ATOMIC_SHARED_POINTER_LOCK_FREE_STACK_H

#include <optional>
#include "../src/decl_fwd.h"

namespace lu {
    template <class TValue>
    class LockFreeStack {
        struct Node {
            TValue value;
            SharedPtr<Node> next{};

            template <class... Args>
            Node(Args &&... args) : value(std::forward<Args>(args)...) {}
        };

    public:
        void push(const TValue &value) {
            SharedPtr<Node> new_node = makeShared<Node>(value);
            do {
                new_node->next = head_.load();
            } while (!head_.compareExchange(new_node->next, new_node));
        }

        std::optional<TValue> pop() {
            SharedPtr<Node> head;
            do {
                head = head_.load();
                if (!head) {
                    return std::nullopt;
                }
            } while (!head_.compareExchange(head, head->next));
            return {head->value};
        }

    private:
        AtomicSharedPtr<Node> head_{};
    };
}

#endif //ATOMIC_SHARED_POINTER_LOCK_FREE_STACK_H
