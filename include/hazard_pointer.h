//
// Created by ludaludaed on 19.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_HAZARD_POINTER_H
#define ATOMIC_SHARED_POINTER_HAZARD_POINTER_H

#include "thread_entry_list.h"

namespace lu {
    template <size_t MaxHP = 1, size_t MaxThread = 100, size_t MaxRetired = 100>
    struct DefaultPolicy {
        static constexpr size_t kMaxHP = MaxHP;
        static constexpr size_t kMaxThread = MaxThread;
        static constexpr size_t kMaxRetired = MaxRetired;
    };

    template <class Policy = DefaultPolicy<>, class Allocator = std::allocator<std::byte>>
    class HazardPointerDomain {
        class ThreadData {
        private:
            void *hazards_pointers_[Policy::kMaxHP];
        };

    private:
        HazardPointerDomain() = default;

    public:
        HazardPointerDomain(const HazardPointerDomain &) = delete;

        HazardPointerDomain(HazardPointerDomain &&) = delete;

        HazardPointerDomain &operator=(const HazardPointerDomain &) = delete;

        HazardPointerDomain &operator=(HazardPointerDomain &&) = delete;

        HazardPointerDomain &instance() {
            static HazardPointerDomain instance;
            return instance;
        }

    private:
        EntriesHolder<ThreadData, Allocator> entries_{};
    };
}

#endif //ATOMIC_SHARED_POINTER_HAZARD_POINTER_H
