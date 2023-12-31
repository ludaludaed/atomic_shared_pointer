//
// Created by ludaludaed on 03.08.2023.
//

#ifndef ATOMIC_SHARED_POINTER_LOCK_FREE_QUEUE_H
#define ATOMIC_SHARED_POINTER_LOCK_FREE_QUEUE_H

#include "../src/decl_fwd.h"
#include <optional>

namespace lu {
    template <class TValue>
    class LockFreeQueue {
    public:
        struct Node {
            TValue value{};
            AtomicSharedPtr<Node> next{};

            template <class... Args>
            Node(Args &&...args) : value(std::forward<Args>(args)...) {}
        };

    public:
        LockFreeQueue() {
            SharedPtr<Node> dummy = makeShared<Node>();
            head_.store(dummy);
            tail_.store(dummy);
        }

        void push(const TValue &value) {
            SharedPtr<Node> new_node = makeShared<Node>(value);
            SharedPtr<Node> cur_tail;
            while (true) {
                cur_tail = tail_.load();
                if (cur_tail->next.load()) {
                    tail_.compareExchange(cur_tail, cur_tail->next.load());
                } else {
                    SharedPtr<Node> null;
                    if (cur_tail->next.compareExchange(null, new_node)) {
                        break;
                    }
                }
            }
            tail_.compareExchange(cur_tail, new_node);
        }

        std::optional<TValue> pop() {
            SharedPtr<Node> cur_head;
            while (true) {
                cur_head = head_.load();
                SharedPtr<Node> cur_head_next = cur_head->next.load();
                if (!cur_head_next) {
                    return std::nullopt;
                }
                if (head_.compareExchange(cur_head, cur_head_next)) {
                    return {std::move(cur_head_next->value)};
                }
            }
        }

    private:
        AtomicSharedPtr<Node> head_;
        AtomicSharedPtr<Node> tail_;
    };
}// namespace lu

#endif//ATOMIC_SHARED_POINTER_LOCK_FREE_QUEUE_H
