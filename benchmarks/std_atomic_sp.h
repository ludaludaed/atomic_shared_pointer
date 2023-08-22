//
// Created by ludaludaed on 02.08.2023.
//

#ifndef ATOMIC_SHARED_POINTER_STD_ATOMIC_SP_H
#define ATOMIC_SHARED_POINTER_STD_ATOMIC_SP_H

#include <memory>
#include <atomic>
#include <optional>

namespace std_atomic_sp {
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

    template <class TValue>
    class LockFreeQueue {
        struct Node {
            TValue value{};
            std::atomic<std::shared_ptr<Node>> next{};

            template <class... Args>
            Node(Args &&... args) : value(std::forward<Args>(args)...) {}
        };

    public:
        LockFreeQueue() {
            std::shared_ptr<Node> dummy = std::make_shared<Node>();
            head_.store(dummy);
            tail_.store(dummy);
        }

        void push(const TValue &value) {
            std::shared_ptr<Node> new_node = std::make_shared<Node>(value);
            std::shared_ptr<Node> cur_tail;
            while (true) {
                cur_tail = tail_.load();
                if (cur_tail->next.load()) {
                    tail_.compare_exchange_strong(cur_tail, cur_tail->next.load());
                } else {
                    std::shared_ptr<Node> null;
                    if (cur_tail->next.compare_exchange_strong(null, new_node)) {
                        break;
                    }
                }
            }
            tail_.compare_exchange_strong(cur_tail, new_node);
        }

        std::optional<TValue> pop() {
            std::shared_ptr<Node> cur_head;
            while (true) {
                cur_head = head_.load();
                std::shared_ptr<Node> cur_head_next = cur_head->next.load();
                if (!cur_head_next) {
                    return std::nullopt;
                }
                if (head_.compare_exchange_strong(cur_head, cur_head_next)) {
                    return {std::move(cur_head_next->value)};
                }
            }
        }

    private:
        std::atomic<std::shared_ptr<Node>> head_;
        std::atomic<std::shared_ptr<Node>> tail_;
    };
}


#endif //ATOMIC_SHARED_POINTER_STD_ATOMIC_SP_H
