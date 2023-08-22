//
// Created by ludaludaed on 09.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_ATOMIC_SHARED_POINTER_H
#define ATOMIC_SHARED_POINTER_ATOMIC_SHARED_POINTER_H

#include "utils.h"
#include <atomic>
#include <memory>


namespace lu::detail {
    class ControlBlockBase {
    protected:
        ControlBlockBase() : ref_counter_(1), weak_counter_(1) {}

    public:
        virtual ~ControlBlockBase() = default;

        ControlBlockBase(const ControlBlockBase &) = delete;

        ControlBlockBase &operator=(const ControlBlockBase &) = delete;

    public:
        bool incrementNotZeroRef(size_t number_of_refs = 1) {
            size_t ref_count = ref_counter_.load();
            while (ref_count != 0) {
                if (ref_counter_.compare_exchange_strong(ref_count, ref_count + number_of_refs)) {
                    return true;
                }
            }
            return false;
        }

        void incrementRef(size_t number_of_refs = 1) {
            ref_counter_.fetch_add(number_of_refs);
        }

        void incrementWeakRef(size_t number_of_refs = 1) {
            weak_counter_.fetch_add(number_of_refs);
        }

        void decrementRef(size_t number_of_refs = 1) {
            if (ref_counter_.fetch_sub(number_of_refs) == 1) {
                destroy();
                decrementWeakRef();
            }
        }

        void decrementWeakRef(size_t number_of_refs = 1) {
            if (weak_counter_.fetch_sub(number_of_refs) == 1) {
                deleteThis();
            }
        }

        size_t useCount() const {
            return ref_counter_.load(std::memory_order_relaxed);
        }

        virtual void *get() = 0;

    private:
        virtual void destroy() = 0;

        virtual void deleteThis() = 0;

    public:
        ControlBlockBase* Next{nullptr};

    private:
        std::atomic<size_t> ref_counter_;
        std::atomic<size_t> weak_counter_;
    };

    template <class TValue, class Deleter, class Allocator>
    class ControlBlock : public ControlBlockBase {
    private:
        using InternalAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<ControlBlock>;

    public:
        explicit ControlBlock(TValue *value, Deleter deleter, const Allocator &allocator)
            : ControlBlockBase(),
              value_(value),
              deleter_(std::move(deleter)),
              allocator_(allocator) {}

        ~ControlBlock() override = default;

        void *get() override {
            return value_;
        }

        static ControlBlock *create(TValue *value, Deleter deleter, const Allocator &allocator) {
            DeleterGuard guard(value, deleter);
            InternalAllocator internal_allocator(allocator);
            AllocateGuard allocation(internal_allocator);
            allocation.allocate();
            allocation.construct(value, std::move(deleter), allocator);
            guard.release();
            return allocation.release();
        }

    private:
        void destroy() override {
            deleter_(value_);
        }

        void deleteThis() override {
            using AllocatorTraits = std::allocator_traits<InternalAllocator>;
            InternalAllocator allocator = allocator_;
            this->~ControlBlock();
            AllocatorTraits::deallocate(allocator, this, 1);
        }

    private:
        TValue *value_;
        Deleter deleter_;
        InternalAllocator allocator_;
    };

    template <class TValue, class Destructor, class Allocator>
    class InplaceControlBlock : public ControlBlockBase {
    private:
        using InternalAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<InplaceControlBlock>;

    public:
        template <class... Args>
        explicit InplaceControlBlock(Destructor destructor, const Allocator &allocator, Args &&...args)
            : ControlBlockBase(),
              destructor_(std::move(destructor)),
              allocator_(allocator) {
            value_.construct(std::forward<Args>(args)...);
        }

        ~InplaceControlBlock() override = default;

        void *get() override {
            return &value_;
        }

        template <class... Args>
        static InplaceControlBlock *create(Destructor destructor, const Allocator &allocator, Args &&...args) {
            InternalAllocator internal_allocator(allocator);
            AllocateGuard allocation(internal_allocator);
            allocation.allocate();
            allocation.construct(std::move(destructor), allocator, std::forward<Args>(args)...);
            return allocation.release();
        }

    private:
        void destroy() override {
            destructor_(&value_);
        }

        void deleteThis() override {
            using AllocatorTraits = std::allocator_traits<InternalAllocator>;
            InternalAllocator allocator = allocator_;
            this->~InplaceControlBlock();
            AllocatorTraits::deallocate(allocator, this, 1);
        }

    private:
        AlignedStorage<TValue> value_;
        Destructor destructor_;
        InternalAllocator allocator_;
    };

    template <class TValue>
    class SharedPtr;

    template <class TValue>
    class WeakPtr;

    template <class TValue>
    class SharedPtr {
        template <class TTValue, class Allocator, class... Args>
        friend SharedPtr<TTValue> allocateShared(const Allocator &allocator, Args &&...args);

        template <class TTValue, class... Args>
        friend SharedPtr<TTValue> makeShared(Args &&...args);

        template <class TTValue>
        friend class WeakPtr;

        template <class TTValue>
        friend class SharedPtr;

        template <class TTValue, class Reclaimer>
        friend class AtomicSharedPtr;

    public:
        using element_type = TValue;

    private:
        // only for atomic shared pointer
        explicit SharedPtr(ControlBlockBase *control_block)
            : control_block_(control_block),
              value_(reinterpret_cast<TValue *>(control_block->get())) {}

        ControlBlockBase *release() {
            auto old = control_block_;
            control_block_ = nullptr;
            value_ = nullptr;
            return old;
        }

    public:
        SharedPtr() : control_block_(nullptr), value_(nullptr) {}

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        explicit SharedPtr(TTValue *value) {
            construct(value);
        }

        template <class TTValue, class Deleter, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        SharedPtr(TTValue *value, Deleter deleter) {
            construct(value, std::move(deleter));
        }

        template <class TTValue, class Deleter, class Allocator, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        SharedPtr(TTValue *value, Deleter deleter, Allocator &allocator) {
            construct(value, std::move(deleter), allocator);
        }

        SharedPtr(const SharedPtr &other)
            : control_block_(other.control_block_), value_(other.value_) {
            if (control_block_ != nullptr) {
                control_block_->incrementRef();
            }
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        SharedPtr(const SharedPtr<TTValue> &other)
            : control_block_(other.control_block_), value_(other.value_) {
            if (control_block_ != nullptr) {
                control_block_->incrementRef();
            }
        }

        SharedPtr(SharedPtr &&other) noexcept : control_block_(other.control_block_), value_(other.value_) {
            other.control_block_ = nullptr;
            other.value_ = nullptr;
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        SharedPtr(SharedPtr<TTValue> &&other)
            : control_block_(other.control_block_), value_(other.value_) {
            other.control_block_ = nullptr;
            other.value_ = nullptr;
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        explicit SharedPtr(const WeakPtr<TTValue> &other) {
            if (other.control_block_ != nullptr && other.control_block_->incrementNotZeroRef()) {
                control_block_ = other.control_block_;
                value_ = other.value_;
            }
        }

        ~SharedPtr() {
            thread_local ControlBlockBase* head{nullptr};
            thread_local bool in_progress {false};
            
            if (control_block_ == nullptr) {
                return;
            }

            control_block_->Next = head;
            head = control_block_;

            if (!in_progress) {
                in_progress = true;
                while (head != nullptr) {
                    auto delete_control_block = head;
                    head = head->Next;
                    delete_control_block->decrementRef();
                }
                in_progress = false;
            }
        }

        SharedPtr &operator=(const SharedPtr &other) {
            SharedPtr temp(other);
            swap(temp);
            return *this;
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        SharedPtr &operator=(const SharedPtr<TTValue> &other) {
            SharedPtr temp(other);
            swap(temp);
            return *this;
        }

        SharedPtr &operator=(SharedPtr &&other) noexcept {
            SharedPtr temp(std::move(other));
            swap(temp);
            return *this;
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        SharedPtr &operator=(SharedPtr<TTValue> &&other) {
            SharedPtr temp(std::move(other));
            swap(temp);
            return *this;
        }

        void swap(SharedPtr &other) {
            std::swap(control_block_, other.control_block_);
            std::swap(value_, other.value_);
        }

        void reset() {
            SharedPtr temp;
            swap(temp);
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        void reset(TTValue *value) {
            SharedPtr temp(value);
            swap(temp);
        }

        template <class TTValue, class Deleter, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        void reset(TTValue *value, Deleter deleter) {
            SharedPtr temp(value, std::move(deleter));
            swap(temp);
        }

        template <class TTValue, class Deleter, class Allocator, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        void reset(TTValue *value, Deleter &deleter, Allocator &allocator) {
            SharedPtr temp(value, deleter, allocator);
            swap(temp);
        }

        explicit operator bool() const {
            return control_block_ != nullptr;
        }

        TValue &operator*() const {
            return *value_;
        }

        TValue *operator->() const {
            return value_;
        }

        [[nodiscard]] long useCount() const {
            if (control_block_ != nullptr) {
                return control_block_->useCount();
            } else {
                return 0;
            }
        }

        template <class TTValue>
        bool operator==(const SharedPtr<TTValue> &other) {
            return control_block_->get() == other.control_block_->get();
        }

        template <class TTValue>
        bool operator!=(const SharedPtr<TTValue> &other) {
            return control_block_->get() != other.control_block_->get();
        }

        template <class TTValue>
        bool operator<(const SharedPtr<TTValue> &other) {
            return control_block_->get() < other.control_block_->get();
        }

        template <class TTValue>
        bool operator>(const SharedPtr<TTValue> &other) {
            return control_block_->get() < other.control_block_->get();
        }

        template <class TTValue>
        bool operator<=(const SharedPtr<TTValue> &other) {
            return control_block_->get() < other.control_block_->get();
        }

        template <class TTValue>
        bool operator>=(const SharedPtr<TTValue> &other) {
            return control_block_->get() < other.control_block_->get();
        }

    private:
        template <class TTValue, class Deleter = DefaultDeleter, class Allocator = std::allocator<TTValue>>
        void construct(TTValue *value, Deleter deleter = Deleter{}, const Allocator &allocator = Allocator{}) {
            using ControlBlock = ControlBlock<TTValue, Deleter, Allocator>;
            auto control_block = ControlBlock::create(value, std::move(deleter), allocator);
            setPointers(value, control_block);
        }

        template <class TTValue>
        void setPointers(TTValue *value, ControlBlockBase *control_block) {
            control_block_ = control_block;
            value_ = value;
        }

    private:
        ControlBlockBase *control_block_{nullptr};
        TValue *value_{nullptr};
    };

    template <class TValue, class Allocator, class... Args>
    SharedPtr<TValue> allocateShared(const Allocator &allocator, Args &&...args) {
        using ControlBlock = InplaceControlBlock<TValue, DefaultDestructor, Allocator>;
        auto control_block = ControlBlock::create(DefaultDestructor{}, allocator, std::forward<Args>(args)...);
        SharedPtr<TValue> result;
        result.setPointers(reinterpret_cast<TValue *>(control_block->get()), control_block);
        return std::move(result);
    }

    template <class TValue, class... Args>
    SharedPtr<TValue> makeShared(Args &&...args) {
        return std::move(allocateShared<TValue>(std::allocator<TValue>{}, std::forward<Args>(args)...));
    }

    template <class TValue>
    class WeakPtr {
        template <class TTValue>
        friend class WeakPtr;

        template <class TTValue>
        friend class SharedPtr;

    public:
        using element_type = TValue;

    public:
        WeakPtr() = default;

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        explicit WeakPtr(const SharedPtr<TTValue> &other)
            : control_block_(other.control_block_), value_(other.value_) {
            if (control_block_ != nullptr) {
                control_block_->incrementWeakRef();
            }
        }

        WeakPtr(const WeakPtr &other)
            : control_block_(other.control_block_), value_(other.value_) {
            if (control_block_ != nullptr) {
                control_block_->incrementWeakRef();
            }
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        WeakPtr(const WeakPtr<TTValue> &other)
            : control_block_(other.control_block_), value_(other.value_) {
            if (control_block_ != nullptr) {
                control_block_->incrementWeakRef();
            }
        }

        WeakPtr(WeakPtr &&other) noexcept : control_block_(other.control_block_), value_(other.value_) {
            other.control_block_ = nullptr;
            other.value_ = nullptr;
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        WeakPtr(WeakPtr<TTValue> &&other)
            : control_block_(other.control_block_), value_(other.value_) {
            other.control_block_ = nullptr;
            other.value_ = nullptr;
        }

        ~WeakPtr() {
            if (control_block_ != nullptr) {
                control_block_->decrementWeakRef();
            }
        }

        WeakPtr &operator=(const WeakPtr &other) {
            WeakPtr temp(other);
            swap(temp);
            return *this;
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        WeakPtr &operator=(const WeakPtr<TTValue> &other) {
            WeakPtr temp(other);
            swap(temp);
            return *this;
        }

        WeakPtr &operator=(WeakPtr &&other) noexcept {
            WeakPtr temp(std::move(other));
            swap(temp);
            return *this;
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        WeakPtr &operator=(WeakPtr<TTValue> &&other) {
            WeakPtr temp(std::move(other));
            swap(temp);
            return *this;
        }

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        WeakPtr &operator=(const SharedPtr<TTValue> &other) {
            WeakPtr temp(other);
            swap(temp);
            return *this;
        }

        void reset() {
            WeakPtr temp;
            swap(temp);
        }

        void swap(WeakPtr &other) {
            std::swap(control_block_, other.control_block_);
            std::swap(value_, other.value_);
        }

        [[nodiscard]] bool expired() const {
            return control_block_ != nullptr;
        }

        [[nodiscard]] long useCount() const {
            if (control_block_ != nullptr) {
                return control_block_->useCount();
            } else {
                return 0;
            }
        }

        SharedPtr<TValue> lock() const {
            SharedPtr<TValue> result(*this);
            return std::move(result);
        }

    private:
        ControlBlockBase *control_block_{nullptr};
        TValue *value_{nullptr};
    };


    template <class TValue, class Reclaimer>
    class AtomicSharedPtr {
    public:
        static constexpr bool is_always_lock_free = true;

    public:
        AtomicSharedPtr() : control_block_(nullptr) {}

        AtomicSharedPtr(const AtomicSharedPtr &) = delete;

        AtomicSharedPtr(AtomicSharedPtr &&) = delete;

        ~AtomicSharedPtr() {
            thread_local ControlBlockBase* head{nullptr};
            thread_local bool in_progress {false};

            auto ptr = control_block_.load();
            
            if (ptr == nullptr) {
                return;
            }

            ptr->Next = head;
            head = ptr;

            if (!in_progress) {
                in_progress = true;
                while (head != nullptr) {
                    auto delete_control_block = head;
                    head = head->Next;
                    delete_control_block->decrementRef();
                }
                in_progress = false;
            }
        }

        AtomicSharedPtr &operator=(const AtomicSharedPtr &) = delete;

        AtomicSharedPtr &operator=(AtomicSharedPtr &&) = delete;

        AtomicSharedPtr &operator=(SharedPtr<TValue> other) {
            store(std::move(other));
            return *this;
        }

        [[nodiscard]] bool is_lock_free() const noexcept {
            return true;
        }

        void store(SharedPtr<TValue> ptr, std::memory_order order = std::memory_order_seq_cst) {
            ControlBlockBase *new_ptr = ptr.release();
            ControlBlockBase *old_ptr = control_block_.exchange(new_ptr, order);
            if (old_ptr != nullptr) {
                Reclaimer::delayDecrementRef(old_ptr);
            }
        }

        SharedPtr<TValue> load() const {
            auto guarded = Reclaimer::protect(control_block_);
            if (guarded.get() == nullptr) {
                return SharedPtr<TValue>{};
            } else {
                guarded->incrementRef();
                return SharedPtr<TValue>(guarded.get());
            }
        }

        SharedPtr<TValue> exchange(SharedPtr<TValue> ptr, std::memory_order order = std::memory_order_seq_cst) {
            ControlBlockBase *new_ptr = ptr.release();
            ControlBlockBase *old_ptr = control_block_.exchange(new_ptr, order);
            return SharedPtr<TValue>(old_ptr);
        }

        bool compareExchange(SharedPtr<TValue> &expected, SharedPtr<TValue> desired) {
            ControlBlockBase *expected_ptr = expected.control_block_;
            ControlBlockBase *desired_ptr = desired.control_block_;
            if (control_block_.compare_exchange_strong(expected_ptr, desired_ptr)) {
                if (expected_ptr != nullptr) {
                    Reclaimer::delayDecrementRef(expected_ptr);
                }
                desired.release();
                return true;
            } else {
                expected = std::move(load());
                return false;
            }
        }

    private:
        std::atomic<ControlBlockBase *> control_block_;
    };
}// namespace lu::detail


#endif//ATOMIC_SHARED_POINTER_ATOMIC_SHARED_POINTER_H
