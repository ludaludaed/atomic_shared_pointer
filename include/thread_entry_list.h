//
// Created by ludaludaed on 25.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_THREAD_ENTRY_LIST_H
#define ATOMIC_SHARED_POINTER_THREAD_ENTRY_LIST_H

#include <memory>
#include "utils.h"

namespace lu {
    template <class TValue, class Allocator = std::allocator<TValue>>
    class ThreadEntryList {
    public:
        class Entry {
            friend class ThreadEntryList;

            friend class Iterator;

        private:
            Entry() = default;

        public:
            Entry(const Entry &) = delete;

            Entry(Entry &&) = delete;

            Entry &operator=(const Entry &) = delete;

            Entry &operator=(Entry &&) = delete;

            void release() {
                active_.store(false, std::memory_order_release);
            }

            bool isActive() const {
                return active_.load(std::memory_order_relaxed);
            }

            TValue &value() {
                return value_;
            }

            const TValue &value() const {
                return value_;
            }

        private:
            TValue value_{};
            Entry *next_{nullptr};
            std::atomic<bool> active_{true};
        };

        class Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = Entry;
            using difference_type = std::ptrdiff_t;
            using reference = value_type &;
            using pointer = value_type *;

        public:
            Iterator() : current_(nullptr) {}

            explicit Iterator(Entry *current) : current_(current) {}

            reference operator*() const {
                return *current_;
            }

            pointer operator->() const {
                return &current_;
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
        ThreadEntryList() = default;

        explicit ThreadEntryList(Allocator &allocator) : allocator_(allocator) {}

        ThreadEntryList(const ThreadEntryList &) = delete;

        ThreadEntryList(ThreadEntryList &&) = delete;

        ThreadEntryList &operator=(const ThreadEntryList &) = delete;

        ThreadEntryList &operator=(ThreadEntryList &&) = delete;

        ~ThreadEntryList() {
            clear();
        }

        Entry *acquireEntry() {
            Entry *acquired_entry = findFree();
            if (acquired_entry == nullptr) {
                acquired_entry = allocItem();
                internalPush(acquired_entry);
            }
            return acquired_entry;
        }

        void releaseEntry(Entry *entry) const {
            if (entry == nullptr) {
                return;
            }
            entry->active_.store(false);
        }

        iterator begin() const {
            return iterator(head_.load());
        }

        iterator end() const {
            return iterator();
        }

    private:
        Entry *allocItem() {
            AllocateGuard al(allocator_);
            al.allocate();
            ::new(al.ptr()) Entry();
            return al.release();
        }

        void internalPush(Entry *node) {
            Entry *head = head_.load();
            do {
                node->next_ = head;
            } while (head_.compare_exchange_strong(head, node));
        }

        Entry *findFree() const {
            Entry *current = head_.load();
            while (current != nullptr) {
                if (!current->active_.exchange(true)) {
                    return current;
                }
                current = current->next_;
            }
            return nullptr;
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

    private:
        InternalAllocator allocator_;
        std::atomic<Entry *> head_{nullptr};
    };

    template <class TValue, class Destructor = DefaultDestructor, class Allocator = std::allocator<TValue>>
    class EntriesHolder {
        friend class EntryHolder;

    public:
        using Entry = ThreadEntryList<TValue, Allocator>::Entry;
        using iterator = ThreadEntryList<TValue, Allocator>::iterator;
    private:
        class EntryHolder {
            friend class EntriesHolder;

        private:
            EntryHolder() = default;

        public:
            EntryHolder(const EntryHolder &) = delete;

            EntryHolder(EntryHolder &&) = delete;

            EntryHolder &operator=(const EntryHolder &) = delete;

            EntryHolder &operator=(EntryHolder &&) = delete;

            ~EntryHolder() {
                if (entry_ != nullptr) {
                    Destructor destructor;
                    destructor(&entry_->value());
                    entry_->release();
                }
            }

            TValue &getValue() {
                if (entry_ == nullptr) {
                    entry_ = list_.acquireEntry();
                }
                return entry_->value();
            }

            Entry &getEntry() {
                if (entry_ == nullptr) {
                    entry_ = list_.acquireEntry();
                }
                return &entry_;
            }

        private:
            ThreadEntryList<TValue, Allocator>::Entry *entry_;
        };

    public:
        EntriesHolder() = default;

        TValue &getValue() {
            EntryHolder & holder = getHolder();
            return holder.getValue();
        }

        Entry &getEntry() {
            EntryHolder & holder = getHolder();
            return holder.getEntry();
        }

        iterator begin() {
            return list_.begin();
        }

        iterator end() {
            return list_.end();
        }

    private:
        EntryHolder &getHolder() {
            thread_local EntryHolder instance;
            return instance;
        }

    private:
        static ThreadEntryList<TValue, Allocator> list_;
    };

    template <class TValue, class Destructor, class Allocator>
    ThreadEntryList<TValue, Allocator> EntriesHolder<TValue, Destructor, Allocator>::list_ = {};
} // namespace lu
#endif //ATOMIC_SHARED_POINTER_THREAD_ENTRY_LIST_H
