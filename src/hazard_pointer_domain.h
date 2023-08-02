//
// Created by ludaludaed on 19.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_HAZARD_POINTER_DOMAIN_H
#define ATOMIC_SHARED_POINTER_HAZARD_POINTER_DOMAIN_H

#include <bitset>
#include <cassert>
#include <algorithm>
#include <thread>
#include "thread_entry_list.h"

namespace lu {
    namespace detail {
        using hazard_ptr_t = void *;
        using retired_ptr_t = void *;

        template <size_t MaxHP>
        class HazardPtrList {
        public:
            class HazardPtr {
                friend HazardPtrList;

            public:
                HazardPtr() = default;

                void clear() {
                    hazard_ptr_.store(nullptr);
                }

                template <class TValue>
                void store(TValue *hazard_ptr) {
                    hazard_ptr_.store(reinterpret_cast<hazard_ptr_t>(hazard_ptr));
                }

                hazard_ptr_t load() const {
                    return hazard_ptr_.load();
                }

                template <class TValue>
                TValue *loadAs() const {
                    return reinterpret_cast<TValue *>(hazard_ptr_.load());
                }

            private:
                std::atomic<hazard_ptr_t> hazard_ptr_{nullptr};
                HazardPtr *next_{nullptr};
            };

        public:
            HazardPtrList() : free_(hazards_) {
                for (HazardPtr *it = hazards_; it < hazards_ + MaxHP - 1; ++it) {
                    it->next_ = it + 1;
                }
            }

            HazardPtr *acquire() {
                assert(!full());
                HazardPtr *result = free_;
                free_ = free_->next_;
                return result;
            }

            void release(HazardPtr *hazard) {
                assert(hazard >= hazards_ && hazard < hazards_ + MaxHP);
                hazard->clear();
                hazard->next_ = free_;
                free_ = hazard;
            }

            void clear() {
                free_ = hazards_;
                for (HazardPtr *it = hazards_; it < hazards_ + MaxHP; ++it) {
                    it->clear();
                    it->next_ = it + 1;
                }
                HazardPtr *last = hazards_ + MaxHP - 1;
                last->next_ = nullptr;
            }

            HazardPtr *begin() {
                return hazards_;
            }

            HazardPtr *end() {
                return hazards_ + MaxHP;
            }

            [[maybe_unused]] bool full() const {
                return free_ == nullptr;
            }

        private:
            HazardPtr hazards_[MaxHP]{};
            HazardPtr *free_{nullptr};
        };

        template <size_t MaxRetired>
        class RetiredList {
        public:
            class RetiredPtr {
                typedef void (*DisposerFunc )(retired_ptr_t);

            public:
                RetiredPtr() = default;

                RetiredPtr(retired_ptr_t pointer, DisposerFunc dispose)
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
                retired_ptr_t pointer_{nullptr};
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
                *last_ = std::move(retired);
                last_ += 1;
            }

            void setLast(RetiredPtr *new_last) {
                assert(new_last >= retires_ && new_last <= retires_ + MaxRetired);
                last_ = new_last;
            }

            void clear() {
                last_ = retires_;
            }

            RetiredPtr *begin() {
                return retires_;
            }

            RetiredPtr *end() {
                return last_;
            }

        private:
            RetiredPtr retires_[MaxRetired]{};
            RetiredPtr *last_;
        };
    } // namespace detail

    template <size_t MaxHP = 4, size_t MaxRetired = 256, size_t ScanDelay = 8>
    struct HazardPointersGenericPolicy {
        static constexpr size_t kMaxHP = MaxHP;
        static constexpr size_t kMaxRetired = MaxRetired;
        static constexpr size_t kScanDelay = ScanDelay;
    };

    template <class Policy = HazardPointersGenericPolicy<4, 256, 8>, class Allocator = std::allocator<std::byte>>
    class HazardPointerDomain {
        friend class DestructThreadEntry;

        friend class GuardedPtr;

        using HazardPointers = detail::HazardPtrList<Policy::kMaxHP>;
        using RetiredPointers = detail::RetiredList<Policy::kMaxRetired>;
        using HazardPtr = HazardPointers::HazardPtr;
        using RetiredPtr = RetiredPointers::RetiredPtr;

        class ThreadData {
        public:
            ThreadData() = default;

            void releaseHP(HazardPtr *ptr) {
                hazards.release(ptr);
                if (++ticks % Policy::kScanDelay == 0) {
                    HazardPointerDomain::instance().scan();
                }
            }

            HazardPtr *acquireHP() {
                return hazards.acquire();
            }

        public:
            size_t ticks{0};
            HazardPointers hazards{};
            RetiredPointers retires{};
        };

        struct DestructThreadEntry {
            void operator()(ThreadData *data) const {
                data->hazards.clear();
                HazardPointerDomain::instance().scan();
            }
        };

    public:
        template <class TValue>
        class GuardedPtr {
        public:
            GuardedPtr() = default;

            GuardedPtr(TValue *value, HazardPtr *hazard_ptr) : value_(value), hazard_ptr_(hazard_ptr) {}

            GuardedPtr(const GuardedPtr &) = delete;

            GuardedPtr(GuardedPtr &&other)
            noexcept: value_(other.value_), hazard_ptr_(other.hazard_ptr_) {
                other.value_ = nullptr;
                other.hazard_ptr_ = nullptr;
            }

            ~GuardedPtr() {
                clearProtection();
            }

            GuardedPtr &operator=(const GuardedPtr &) = delete;

            GuardedPtr &operator=(GuardedPtr &&other) noexcept {
                GuardedPtr temp(std::move(other));
                swap(temp);
                return *this;
            }

            void swap(GuardedPtr &other) {
                std::swap(value_, other.value_);
                std::swap(hazard_ptr_, other.hazard_ptr_);
            }

            explicit operator bool() const {
                return hazard_ptr_ != nullptr;
            }

            TValue &operator*() {
                return *value_;
            }

            const TValue &operator*() const {
                return *value_;
            }

            TValue *operator->() {
                return value_;
            }

            TValue *get() {
                return value_;
            }

            void clear() {
                clearProtection();
                value_ = nullptr;
                hazard_ptr_ = nullptr;
            }

            void clearProtection() {
                if (hazard_ptr_ != nullptr) {
                    HazardPointerDomain::instance().release(hazard_ptr_);
                }
            }

        private:
            TValue *value_{nullptr};
            HazardPtr *hazard_ptr_{nullptr};
        };

    private:
        HazardPointerDomain() = default;

    public:
        HazardPointerDomain(const HazardPointerDomain &) = delete;

        HazardPointerDomain(HazardPointerDomain &&) = delete;

        HazardPointerDomain &operator=(const HazardPointerDomain &) = delete;

        HazardPointerDomain &operator=(HazardPointerDomain &&) = delete;

        ~HazardPointerDomain() {
            for (auto it = entries_.begin(); it != entries_.end(); ++it) {
                ThreadData &data = it->value();
                for (auto ret_it = data.retires.begin(); ret_it != data.retires.end(); ++ret_it) {
                    if (*ret_it) {
                        ret_it->dispose();
                    }
                }
            }
        }

        static HazardPointerDomain &instance() {
            static HazardPointerDomain instance;
            return instance;
        }

        void release(HazardPtr *hazard_ptr) {
            if (hazard_ptr != nullptr) {
                ThreadData &thread_data = entries_.getValue();
                hazard_ptr->clear();
                thread_data.releaseHP(hazard_ptr);
            }
        }

        template <class TValue>
        GuardedPtr<TValue> protect(const std::atomic<TValue *> &ptr) {
            ThreadData &thread_data = entries_.getValue();
            HazardPtr *hazard_ptr = thread_data.acquireHP();
            TValue *result;
            do {
                result = ptr.load();
                hazard_ptr->store(result);
            } while (result != ptr.load());
            return GuardedPtr<TValue>(result, hazard_ptr);
        }

        template <class Disposer, class TValue>
        void retire(TValue *ptr) {
            struct TypeRecovery {
                static void dispose(void *value) {
                    Disposer()(reinterpret_cast<TValue *>(value));
                }
            };
            ThreadData &thread_data = entries_.getValue();
            // TODO: Maybe dead-lock if T * HP > R
            while (thread_data.retires.full()) {
                scan(thread_data);
                std::this_thread::yield();
            }
            RetiredPtr retired(ptr, TypeRecovery::dispose);
            thread_data.retires.pushBack(std::move(retired));
        }

    private:
        void scan() {
            ThreadData &thread_data = entries_.getValue();
            scan(thread_data);
            helpScan(thread_data);
        }

        void scan(ThreadData &thread_data) {
            if (thread_data.retires.empty()) {
                return;
            }
            std::bitset<Policy::kMaxRetired> hazards_;
            RetiredPtr *ret_beg = thread_data.retires.begin();
            RetiredPtr *ret_end = thread_data.retires.end();
            std::sort(ret_beg, ret_end);
            for (auto thread_it = entries_.begin(); thread_it != entries_.end(); ++thread_it) {
                ThreadData &other_td = thread_it->value();
                for (auto hp = other_td.hazards.begin(); hp != other_td.hazards.end(); ++hp) {
                    auto ptr = hp->load();
                    if (ptr != nullptr) {
                        RetiredPtr dummy_retired(ptr, nullptr);
                        RetiredPtr *res = std::lower_bound(ret_beg, ret_end, dummy_retired);
                        if (res != ret_end && *res == dummy_retired) {
                            hazards_[res - ret_beg] = true;
                        }
                    }
                }
            }

            RetiredPtr *ret_insert = ret_beg;
            for (auto it = ret_beg; it != ret_end; ++it) {
                if (!hazards_[it - ret_beg]) {
                    it->dispose();
                } else {
                    if (ret_insert != it) {
                        *ret_insert = *it;
                    }
                    ret_insert += 1;
                }
            }
            thread_data.retires.setLast(ret_insert);
        }

        void helpScan(ThreadData &thread_data) {
            for (auto thread_it = entries_.begin(); thread_it != entries_.end(); ++thread_it) {
                ThreadData &other_td = thread_it->value();
                if (&thread_data == &other_td) {
                    continue;
                }
                if (!thread_it->tryAcquire()) {
                    continue;
                }
                if (other_td.retires.empty()) {
                    thread_it->release();
                    continue;
                }
                RetiredPtr *src_beg = other_td.retires.begin();
                RetiredPtr *src_end = other_td.retires.end();
                for (auto it = src_beg; it != src_end; ++it) {
                    // TODO: Maybe dead-lock if T * HP > R
                    while (thread_data.retires.full()) {
                        scan(thread_data);
                    }
                    thread_data.retires.pushBack(std::move(*it));
                }
                other_td.retires.clear();
                thread_it->release();
                scan(thread_data);
            }
        }

    private:
        EntriesHolder<ThreadData, DestructThreadEntry, Allocator> entries_{};
    };
}

#endif //ATOMIC_SHARED_POINTER_HAZARD_POINTER_DOMAIN_H
