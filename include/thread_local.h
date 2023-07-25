//
// Created by ludaludaed on 19.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_THREAD_LOCAL_H
#define ATOMIC_SHARED_POINTER_THREAD_LOCAL_H

#include <memory>
#include <atomic>
#include <functional>
#include <cassert>
#include "utils.h"

namespace lu {
    template <class TValue>
    struct Node {
        AlignStorage<TValue> value;
        Node *next{nullptr};
        std::atomic<bool> active{true};
    };

    template <class TValue>
    class ListIterator {
        template <class TTValue, class Allocator>
        friend
        class LockFreeGrowList;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = TValue;
        using difference_type = std::ptrdiff_t;
        using reference = value_type &;
        using pointer = value_type *;

    public:
        ListIterator() : current_(nullptr) {}

        explicit ListIterator(Node<TValue> *current) : current_(current) {}

        reference operator*() const {
            return *(current_->value);
        }

        pointer operator->() const {
            return &(current_->value);
        }

        ListIterator &operator++() {
            current_ = current_->next;
            return *this;
        }

        ListIterator operator++(int) {
            ListIterator result = *this;
            current_ = current_->next;
            return result;
        }

        bool operator==(const ListIterator &other) const {
            return current_ == other.current_;
        }

        bool operator!=(const ListIterator &other) const {
            return current_ != other.current_;
        }

    private:
        Node<TValue> *current_;
    };

    template <class TValue, class Allocator = std::allocator<TValue>>
    class LockFreeGrowList {
    public:
        using iterator = ListIterator<TValue>;
        using value_type = TValue;
        using allocator = Allocator;

    private:
        using NodeT = Node<TValue>;
        using InternalAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<NodeT>;
        using AllocatorTraits = std::allocator_traits<InternalAllocator>;

    public:
        LockFreeGrowList()
                : head_(nullptr), allocator_() {}

        explicit LockFreeGrowList(Allocator &allocator)
                : head_(nullptr), allocator_(allocator) {}

        LockFreeGrowList(const LockFreeGrowList &) = delete;

        LockFreeGrowList(LockFreeGrowList &&other) = delete;

        LockFreeGrowList &operator=(const LockFreeGrowList &) = delete;

        LockFreeGrowList &operator=(LockFreeGrowList &&) = delete;

        ~LockFreeGrowList() {
            NodeT *current = head_.load();
            while (current != nullptr) {
                NodeT *del_node = current;
                current = current->next;
                if (del_node->active.exchange(false)) {
                    del_node->value.destruct();
                }
                AllocatorTraits::deallocate(allocator_, del_node, 1);
            }
        }

    public:

        template <class... Args>
        iterator emplace(Args &&... args) {
            NodeT *new_node = findFree();
            if (new_node != nullptr) {
                new_node->value.construct(std::forward<Args>(args)...);
            } else {
                new_node = allocNode();
                new_node->value.construct(std::forward<Args>(args)...);
                internalPush(new_node);
            }
            return iterator(new_node);
        }

        void erase(iterator it) {
            if (it != end() && it.current_->active.load()) {
                it.current_->value.destruct();
                it.current_->active.store(false);
            }
        }

        iterator begin() {
            return iterator(head_.load());
        }

        iterator end() {
            return iterator();
        }

    private:
        NodeT *allocNode() {
            AllocateGuard<InternalAllocator> al(allocator_);
            al.allocate();
            al.construct();
            return al.release();
        }

        void internalPush(NodeT *node) {
            NodeT *head = head_.load();
            do {
                node->next = head;
            } while (head_.compare_exchange_strong(head, node));
        }

        NodeT *findFree() {
            NodeT *current = head_.load();
            while (current != nullptr) {
                if (!current->active.exchange(true)) {
                    return current;
                }
                current = current->next;
            }
            return nullptr;
        }

    private:
        std::atomic<NodeT *> head_;
        InternalAllocator allocator_;
    };

    template <class TValue>
    struct ThreadLocalEntry {
        AlignStorage<TValue> value;

        ThreadLocalEntry() {
            value.construct();
        }
    };

    template <class TValue>
    class ThreadLocal;

    template <class TValue>
    class ThreadLocalRepBase {
        friend class ThreadLocal<TValue>;

        class Iterator {
            using InternalIterator = ListIterator<ThreadLocalEntry<TValue>>;

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = TValue;
            using difference_type = std::ptrdiff_t;
            using reference = value_type &;
            using pointer = value_type *;

        public:
            Iterator() : current_() {}

            explicit Iterator(InternalIterator current) : current_(current) {}

            reference operator*() const {
                return *(current_->value);
            }

            pointer operator->() const {
                return &(current_->value);
            }

            Iterator &operator++() {
                ++current_;
                return *this;
            }

            Iterator operator++(int) {
                Iterator result = *this;
                ++current_;
                return result;
            }

            bool operator==(const Iterator &other) const {
                return current_ == other.current_;
            }

            bool operator!=(const Iterator &other) const {
                return current_ != other.current_;
            }

        private:
            InternalIterator current_;
        };

    public:
        using entry_iterator = ListIterator<ThreadLocalEntry<TValue>>;
        using iterator = Iterator;

    protected:
        ThreadLocalRepBase() {
            assert(singleton == nullptr);
            singleton = this;
        }

    public:
        ThreadLocalRepBase(const ThreadLocalRepBase &) = delete;

        ThreadLocalRepBase(ThreadLocalRepBase &&) = delete;

        ThreadLocalRepBase &operator=(const ThreadLocalRepBase &) = delete;

        ThreadLocalRepBase &operator=(ThreadLocalRepBase &&) = delete;

        virtual ~ThreadLocalRepBase() {
            singleton = nullptr;
        }

    private:
        virtual entry_iterator create() = 0;

        virtual void free(entry_iterator) = 0;

    public:
        virtual iterator begin() = 0;

        virtual iterator end() = 0;

    private:
        static ThreadLocalRepBase *singleton;
    };

    template <typename TValue>
    ThreadLocalRepBase<TValue> *ThreadLocalRepBase<TValue>::singleton = nullptr;

    template <class TValue, class Destructor = DefaultDestructor, class Allocator = std::allocator<TValue>>
    class ThreadLocalRep : private ThreadLocalRepBase<TValue> {
        friend class ThreadLocal<TValue>;

    private:
        using List = LockFreeGrowList<ThreadLocalEntry<TValue>, Allocator>;

    public:
        using iterator = typename ThreadLocalRepBase<TValue>::iterator;
        using entry_iterator = typename ThreadLocalRepBase<TValue>::entry_iterator;

    public:
        ThreadLocalRep()
                : ThreadLocalRepBase<TValue>(), list_() {}

        explicit ThreadLocalRep(Allocator &allocator)
                : ThreadLocalRepBase<TValue>(), list_(allocator) {}

        explicit ThreadLocalRep(Destructor &destructor)
                : ThreadLocalRepBase<TValue>(), list_(), destructor_(destructor) {}

        ThreadLocalRep(Destructor &destructor, Allocator &allocator)
                : ThreadLocalRepBase<TValue>(), list_(allocator), destructor_(destructor) {}

        ~ThreadLocalRep() override = default;

    private:
        entry_iterator create() override {
            return list_.emplace();
        }

        void free(entry_iterator it) override {
            destructor_(&(it->value));
            return list_.erase(it);
        }

    public:
        iterator begin() override {
            return iterator(list_.begin());
        }

        iterator end() override {
            return iterator(list_.end());
        }

    private:
        List list_;
        Destructor destructor_;
    };

    template <class TValue>
    class ThreadLocal {
        using Base = ThreadLocalRepBase<TValue>;
        using EntryIterator = typename ThreadLocalRepBase<TValue>::entry_iterator;

        class ThreadLocalInternal {
            friend class ThreadLocal;

        private:
            ThreadLocalInternal() = default;

        public:
            ~ThreadLocalInternal() {
                if (Base::singleton != nullptr) {
                    Base::singleton->free(my_entry_);
                }
            }

            ThreadLocalInternal(const ThreadLocalInternal &) = delete;

            ThreadLocalInternal(ThreadLocalInternal &&) = delete;

            ThreadLocalInternal &operator=(const ThreadLocalInternal &) = delete;

            ThreadLocalInternal &operator=(ThreadLocalInternal &&) = delete;

            static ThreadLocalInternal &instance() {
                thread_local ThreadLocalInternal instance;
                return instance;
            }

        private:
            EntryIterator my_entry_{};
        };

    public:
        using iterator = typename ThreadLocalRepBase<TValue>::iterator;

    public:
        ThreadLocal() {
            assert(Base::singleton != nullptr);
            ThreadLocalInternal &instance = ThreadLocalInternal::instance();
            if (instance.my_entry_ == EntryIterator{}) {
                instance.my_entry_ = Base::singleton->create();
            }
        }

        TValue &get() {
            ThreadLocalInternal &instance = ThreadLocalInternal::instance();
            if (instance.my_entry_ == EntryIterator{}) {
                instance.my_entry_ = Base::singleton->create();
            }
            return *(instance.my_entry_->value);
        }

        iterator begin() {
            Base::singleton->begin();
        }

        iterator end() {
            Base::singleton->end();
        }
    };
} // namespace lu
#endif //ATOMIC_SHARED_POINTER_THREAD_LOCAL_H
