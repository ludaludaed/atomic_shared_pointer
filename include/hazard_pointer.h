//
// Created by ludaludaed on 19.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_HAZARD_POINTER_H
#define ATOMIC_SHARED_POINTER_HAZARD_POINTER_H

#include <cassert>
#include "thread_entry_list.h"

namespace lu {
    template <size_t MaxHP = 1, size_t MaxThread = 100, size_t MaxRetired = 100>
    struct GenericPolicy {
        static constexpr size_t kMaxHP = MaxHP;
        static constexpr size_t kMaxThread = MaxThread;
        static constexpr size_t kMaxRetired = MaxRetired;
    };

    namespace detail {
        template <size_t MaxHP>
        class HazardPtrList {
            // TODO: hazard ptr lis

        public:
            void clear() {
                // TODO: clear hazard pointers
            }
        };

        template <size_t MaxRetired>
        class RetiredList {
        public:
            class RetiredPtr {
                typedef void (*DisposerFunc )(void *);

            public:
                RetiredPtr() = default;

                RetiredPtr(void *pointer, DisposerFunc dispose)
                        : pointer_(pointer), disposer_(dispose) {}

                RetiredPtr(const RetiredPtr &other)
                        : pointer_(other.pointer_), disposer_(other.disposer_) {}

                RetiredPtr(RetiredPtr &&other)
                noexcept: pointer_(other.pointer_), disposer_(other.disposer_) {
                    other.clear();
                }

                RetiredPtr &operator=(const RetiredPtr &other) {
                    RetiredPtr temp(other);
                    swap(temp);
                    return *this;
                }

                RetiredPtr &operator=(RetiredPtr &&other) noexcept {
                    RetiredPtr temp(std::move(other));
                    swap(temp);
                    return *this;
                }

                explicit operator bool() const {
                    return pointer_ != nullptr;
                }

                bool operator<(const RetiredPtr &other) const {
                    return pointer_ < other.pointer_;
                }

                bool operator>(const RetiredPtr &other) const {
                    return pointer_ > other.pointer_;
                }

                bool operator<=(const RetiredPtr &other) const {
                    return pointer_ <= other.pointer_;
                }

                bool operator>=(const RetiredPtr &other) const {
                    return pointer_ >= other.pointer_;
                }

                bool operator==(const RetiredPtr &other) const {
                    return pointer_ == other.pointer_;
                }

                bool operator!=(const RetiredPtr &other) const {
                    return pointer_ != other.pointer_;
                }

                void swap(RetiredPtr &other) {
                    std::swap(pointer_, other.pointer_);
                    std::swap(disposer_, other.disposer_);
                }

                void dispose() {
                    disposer_(pointer_);
                    clear();
                }

                void clear() {
                    pointer_ = nullptr;
                    disposer_ = nullptr;
                }

            private:
                void *pointer_{nullptr};
                DisposerFunc disposer_{nullptr};
            };

        public:
            RetiredList() : last_(retires_) {}

            [[nodiscard]] bool empty() const {
                return last_ == retires_;
            }

            [[nodiscard]] bool full() const {
                return last_ == retires_ + MaxRetired;
            }

            [[nodiscard]] size_t size() const {
                return last_ - retires_;
            }

            void pushBack(RetiredPtr &&retired) {
                assert(!full());
                last_ = retired;
                last_ += 1;
            }

            void setLast(RetiredPtr *new_last) {
                assert(new_last >= retires_ && new_last <= retires_ + MaxRetired);
                last_ = new_last;
            }

            void clear() {
                last_ = retires_;
            }

            RetiredPtr begin() {
                return retires_;
            }

            RetiredPtr end() {
                return last_;
            }

        private:
            RetiredPtr retires_[MaxRetired]{};
            RetiredPtr *last_;
        };
    } // namespace detail


    template <class Policy = GenericPolicy<>, class Allocator = std::allocator<std::byte>>
    class HazardPointerDomain {
        friend class DestructThreadEntry;

        class ThreadData {
            using HazardPointers = detail::HazardPtrList<Policy::kMaxHP>;
            using RetiredPointers = detail::RetiredList<Policy::kMaxRetired>;

        public:
            HazardPointers hazard_pointers_;
            RetiredPointers retiredPointers_;
        };

        struct DestructThreadEntry {
            void operator()(ThreadData *data) const {
                data->hazard_pointers_.clear();
                HazardPointerDomain::instance().scan();
                HazardPointerDomain::instance().helpScan();
            }
        };

        template <class TValue>
        class GuardedPtr {
            // TODO: RAII for protect ptr
        };

    private:
        HazardPointerDomain() = default;

    public:
        HazardPointerDomain(const HazardPointerDomain &) = delete;

        HazardPointerDomain(HazardPointerDomain &&) = delete;

        HazardPointerDomain &operator=(const HazardPointerDomain &) = delete;

        HazardPointerDomain &operator=(HazardPointerDomain &&) = delete;

        static HazardPointerDomain &instance() {
            static HazardPointerDomain instance;
            return instance;
        }

        template <class TValue>
        GuardedPtr<TValue> protect(std::atomic<TValue> &ptr) {
            // TODO: protection for ptr
        }

        template <class TValue, class Disposer>
        void retire(TValue *ptr) {
            // TODO: retire for ptr
        }

    private:
        void scan() {
            // TODO scan hazard ptrs
        }

        void helpScan() {
            // TODO scan empty entries
        }

    private:
        EntriesHolder<ThreadData, DestructThreadEntry, Allocator> entries_{};
    };
}

#endif //ATOMIC_SHARED_POINTER_HAZARD_POINTER_H
