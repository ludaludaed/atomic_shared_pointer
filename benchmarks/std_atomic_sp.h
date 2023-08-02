//
// Created by ludaludaed on 02.08.2023.
//

#ifndef ATOMIC_SHARED_POINTER_STD_ATOMIC_SP_H
#define ATOMIC_SHARED_POINTER_STD_ATOMIC_SP_H

#include <memory>
#include <atomic>
#include <optional>

namespace default_structure {
    template <class TValue>
    class LockFreeStack {
        struct Node {
            TValue value{};
            std::shared_ptr<Node> next{};
        };

    public:
        void push(const TValue &value) {
            std::shared_ptr<Node> new_node = std::make_shared<Node>();
            new_node->value = value;
            do {
                new_node->next = head_.load();
            } while (!head_.compare_exchange_strong(new_node->next, new_node));
        }

        std::optional<TValue> pop() {
            std::shared_ptr<Node> head = head_.load();
            if (!head) {
                return {};
            }
            while (!head_.compare_exchange_strong(head, head->next)) {
                head = head_.load();
                if (!head) {
                    return {};
                }
            }
            return {head->value};
        }

    private:
        std::atomic<std::shared_ptr<Node>> head_{};
    };
}


#endif //ATOMIC_SHARED_POINTER_STD_ATOMIC_SP_H
