//
// Created by ludaludaed on 19.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_UTILS_H
#define ATOMIC_SHARED_POINTER_UTILS_H

#include <memory>

namespace lu {
    template <class TValue>
    class AlignedStorage {
    public:
        template <class... Args>
        void construct(Args &&... args) {
            ::new(data_) TValue(std::forward<Args>(args)...);
        }

        void destruct() {
            reinterpret_cast<TValue *>(data_)->~TValue();
        }

        TValue &operator*() {
            return *reinterpret_cast<TValue *>(&data_);
        }

        const TValue &operator*() const {
            return *reinterpret_cast<const TValue *>(&data_);
        }

        TValue *operator&() {
            return reinterpret_cast<TValue *>(&data_);
        }

        const TValue *operator&() const {
            return reinterpret_cast<const TValue *>(&data_);
        }

    private:
        alignas(alignof(TValue)) std::byte data_[sizeof(TValue)];
    };

    struct DefaultDeleter {
        template <class TValue>
        void operator()(TValue *value) {
            delete value;
        }
    };

    struct DefaultDestructor {
        template <class TValue>
        void operator()(TValue *value) {
            value->~TValue();
        }
    };

    template <class Ptr, class Deleter = DefaultDeleter>
    class DeleterGuard {
    public:
        DeleterGuard() = delete;

        DeleterGuard(Ptr ptr, Deleter &deleter) : ptr_(ptr), deleter_(deleter) {}

        DeleterGuard(const DeleterGuard &) = delete;

        DeleterGuard(DeleterGuard &&) = delete;

        DeleterGuard &operator=(const DeleterGuard &) = delete;

        DeleterGuard &operator=(DeleterGuard &&) = delete;

        Ptr release() {
            return std::exchange(ptr_, nullptr);
        }

        ~DeleterGuard() {
            if (ptr_) {
                deleter_(ptr_);
            }
        }

    private:
        Ptr ptr_;
        Deleter &deleter_;
    };

    template <class Allocator>
    class AllocateGuard {
        using AllocatorTraits = std::allocator_traits<Allocator>;

    public:
        using pointer = typename AllocatorTraits::pointer;
        using const_pointer = typename AllocatorTraits::const_pointer;

    private:
        Allocator &allocator_;
        pointer pointer_;

    public:
        explicit AllocateGuard(Allocator &allocator)
                : allocator_(allocator), pointer_(nullptr) {}

        AllocateGuard(const AllocateGuard &other) = delete;

        AllocateGuard(AllocateGuard &&other) = delete;

        ~AllocateGuard() {
            if (pointer_ != nullptr) {
                AllocatorTraits::deallocate(allocator_, pointer_, 1);
            }
        }

        AllocateGuard &operator=(const AllocateGuard &other) = delete;

        AllocateGuard &operator=(AllocateGuard &&other) = delete;

        void allocate() {
            pointer_ = nullptr;
            pointer_ = AllocatorTraits::allocate(allocator_, 1);
        }

        template <class... Args>
        void construct(Args &&... args) {
            AllocatorTraits::construct(allocator_, pointer_, std::forward<Args>(args)...);
        }

        pointer release() noexcept {
            pointer released_pointer = pointer_;
            pointer_ = nullptr;
            return released_pointer;
        }

        pointer ptr() noexcept {
            return pointer_;
        }

        const_pointer ptr() const noexcept {
            return pointer_;
        }
    };
} // namespace lu

#endif //ATOMIC_SHARED_POINTER_UTILS_H
