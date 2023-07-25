//
// Created by ludaludaed on 25.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_THREAD_BLOCK_LIST_H
#define ATOMIC_SHARED_POINTER_THREAD_BLOCK_LIST_H

#include <memory>
#include "utils.h"

namespace lu {
    template <class TValue, class Allocator = std::allocator<TValue>>
    class ThreadBlockList {
    public:
        class Entry {
            friend class ThreadBlockList;

            friend class Iterator;

        public:
            Entry() = default;

            bool isActive() const {
                return active_.load(std::memory_order_relaxed);
            }

        private:
            TValue value_;
            Entry *next_;
            std::atomic<bool> active_{true};
        };

        class Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = TValue;
            using difference_type = std::ptrdiff_t;
            using reference = value_type &;
            using pointer = value_type *;

        public:
            Iterator() : current_(nullptr) {}

            explicit Iterator(Entry *current) : current_(current) {}

            reference operator*() const {
                return current_->value_;
            }

            pointer operator->() const {
                return &current_->value_;
            }

            Iterator &operator++() {
                current_ = current_->next_;
                return *this;
            }

            Iterator operator++(int) {
                Iterator result = *this;
                current_ = current_->next_;
                return result;
            }

            bool operator==(const Iterator &other) const {
                return current_ == other.current_;
            }

            bool operator!=(const Iterator &other) const {
                return current_ != other.current_;
            }

        private:
            Entry *current_;
        };

    public:
        using iterator = Iterator;

    private:
        using InternalAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Entry>;
        using AllocatorTraits = std::allocator_traits<InternalAllocator>;

    public:
        ThreadBlockList() = default;

        explicit ThreadBlockList(Allocator &allocator) : allocator_(allocator) {}

        ThreadBlockList(const ThreadBlockList &) = delete;

        ThreadBlockList(ThreadBlockList &&) = delete;

        ThreadBlockList &operator=(const ThreadBlockList &) = delete;

        ThreadBlockList &operator=(ThreadBlockList &&) = delete;

        ~ThreadBlockList() {
            clear();
        }

        void clear() {
            Entry *current = head_.exchange(nullptr);
            while (current != nullptr) {
                Entry *del_entry = current;
                current = current->next_;
                del_entry->~Entry();
                AllocatorTraits::deallocate(allocator_, del_entry, 1);
            }
        }

        Entry *acquireEntry() {
            Entry *acquired_entry = findFree();
            if (acquired_entry == nullptr) {
                acquired_entry = acquireEntry();
                internalPush(acquired_entry);
            }
            return acquired_entry;
        }

        void releaseEntry(Entry *entry) const {
            entry->active_.store(false);
        }

        iterator begin() const {
            return iterator(head_.load());
        }

        iterator end() const {
            return iterator();
        }

    private:
        Entry *allocEntry() const {
            AllocateGuard<InternalAllocator> al(allocator_);
            al.allocate();
            al.construct();
            return al.release();
        }

        void internalPush(Entry *node) {
            Entry *head = head_.load();
            do {
                node->next = head;
            } while (head_.compare_exchange_strong(head, node));
        }

        Entry *findFree() const {
            Entry *current = head_.load();
            while (current != nullptr) {
                if (!current->active.exchange(true)) {
                    return current;
                }
                current = current->next;
            }
            return nullptr;
        }

    private:
        InternalAllocator allocator_;
        std::atomic<Entry *> head_{nullptr};
    };
} // namespace lu
#endif //ATOMIC_SHARED_POINTER_THREAD_BLOCK_LIST_H
