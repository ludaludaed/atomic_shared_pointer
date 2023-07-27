//
// Created by ludaludaed on 09.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_ATOMIC_SHARED_POINTER_H
#define ATOMIC_SHARED_POINTER_ATOMIC_SHARED_POINTER_H

#include <atomic>
#include <memory>
#include "utils.h"

namespace lu {
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
            ref_counter_.fetch_add(number_of_refs);
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

    private:
        std::atomic<size_t> ref_counter_;
        std::atomic<size_t> weak_counter_;
    };

    template <class TValue, class Deleter = DefaultDeleter, class Allocator = std::allocator<TValue>>
    class ControlBlock : public ControlBlockBase {
    private:
        using InternalAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<ControlBlock>;

    public:
        explicit ControlBlock(TValue *value, Deleter &deleter = Deleter{}, const Allocator &allocator = Allocator{})
                : ControlBlockBase(),
                  value_(value),
                  deleter_(std::move(deleter)),
                  allocator_(allocator) {}

        ~ControlBlock() override = default;

        void *get() override {
            return value_;
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

    template <class TValue, class Destructor = DefaultDestructor, class Allocator = std::allocator<TValue>>
    class InplaceControlBlock : public ControlBlockBase {
    private:
        using InternalAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<InplaceControlBlock>;

    public:
        explicit InplaceControlBlock(Destructor destructor = Destructor{}, const Allocator &allocator = Allocator{})
                : ControlBlockBase(),
                  destructor_(std::move(destructor)),
                  allocator_(allocator) {
        }

        ~InplaceControlBlock() override = default;

        template <class... Args>
        void construct(Args &&... args) {
            value_.construct(std::forward<Args>(args)...);
        }

        void *get() override {
            return &value_;
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
        AlignStorage<TValue> value_;
        Destructor destructor_;
        InternalAllocator allocator_;
    };

    template <class TValue>
    class SharedPtr;

    template <class TValue>
    class WeakPtr;

    template <class TValue>
    class SharedPtr {
        template <class TTValue, class ...Args>
        friend SharedPtr<TTValue> makeShared(Args &&... args);

        template <class TTValue, class Allocator, class ...Args>
        friend SharedPtr<TTValue> allocateShared(const Allocator &allocator, Args &&... args);

        template <class TTValue>
        friend
        class WeakPtr;

        template <class TTValue>
        friend
        class SharedPtr;

        template <class TTValue, class Reclaimer>
        friend
        class AtomicSharedPtr;

    public:
        using element_type = TValue;

    private:
        // only for atomic share pointer
        explicit SharedPtr(ControlBlockBase *control_block)
                : control_block_(control_block),
                  value_(reinterpret_cast<TValue>(control_block->get())) {}

    public:
        SharedPtr() : control_block_(nullptr), value_(nullptr) {}

        template <class TTValue, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        explicit SharedPtr(TTValue *value) {
            construct(value);
        }

        template <class TTValue, class Deleter, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        SharedPtr(TTValue *value, Deleter &deleter) {
            construct(value, deleter);
        }

        template <class TTValue, class Deleter, class Allocator, std::enable_if_t<std::is_convertible_v<TTValue *, TValue *>, int> = 0>
        SharedPtr(TTValue *value, Deleter &deleter, Allocator &allocator) {
            construct(value, deleter, allocator);
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

        SharedPtr(SharedPtr &&other)
        noexcept: control_block_(other.control_block_), value_(other.value_) {
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
            if (control_block_ != nullptr) {
                control_block_->decrementRef();
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
        void reset(TTValue *value, Deleter &deleter) {
            SharedPtr temp(value, deleter);
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
        void construct(TTValue *value, const Deleter &deleter = Deleter{}, const Allocator &allocator = Allocator{}) {
            using InternalAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<ControlBlock<TTValue, Deleter, Allocator>>;
            DeleterGuard guard(value, deleter);
            InternalAllocator internal_allocator(allocator);
            AllocateGuard allocation(internal_allocator);
            allocation.allocate();
            allocation.construct(value, const_cast<Deleter &>(deleter), allocator);
            setPointers(value, allocation.release());
            guard.release();
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

    template <class TValue, class... Args>
    SharedPtr<TValue> makeShared(Args &&... args) {
        SharedPtr<TValue> result;
        auto *control_block = new InplaceControlBlock<TValue>();
        control_block->construct(std::forward<Args>(args)...);
        result.setPointers(reinterpret_cast<TValue *>(control_block->get()), control_block);
        return std::move(result);
    }

    template <class TValue, class Allocator, class ...Args>
    SharedPtr<TValue> allocateShared(const Allocator &allocator, Args &&... args) {
        using InternalAllocator = typename std::allocator_traits<Allocator>::template
        rebind_alloc<InplaceControlBlock<TValue, DefaultDestructor, Allocator>>;
        SharedPtr<TValue> result;
        InternalAllocator internal_allocator(allocator);
        AllocateGuard allocation(internal_allocator);
        allocation.allocate();
        allocation.construct(DefaultDestructor{}, allocator);

        allocation.ptr()->construct(std::forward<Args>(args)...);
        result.setPointers(reinterpret_cast<TValue *>(allocation.ptr()->get()), allocation.ptr());
        allocation.release();
        return result;
    }


    template <class TValue>
    class WeakPtr {
        template <class TTValue>
        friend
        class WeakPtr;

        template <class TTValue>
        friend
        class SharedPtr;

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

        WeakPtr(WeakPtr &&other)
        noexcept: control_block_(other.control_block_), value_(other.value_) {
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

        AtomicSharedPtr &operator=(const AtomicSharedPtr &) = delete;

        [[nodiscard]] bool is_lock_free() const noexcept {
            return true;
        }

        void store(SharedPtr<TValue> ptr, std::memory_order order = std::memory_order_seq_cst);

        SharedPtr<TValue> load(std::memory_order order = std::memory_order_seq_cst) const;

        SharedPtr<TValue> exchange(SharedPtr<TValue> ptr, std::memory_order order = std::memory_order_seq_cst);

    private:
        std::atomic<ControlBlockBase *> control_block_;
    };
}

#endif //ATOMIC_SHARED_POINTER_ATOMIC_SHARED_POINTER_H
