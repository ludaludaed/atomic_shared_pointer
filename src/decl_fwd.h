//
// Created by ludaludaed on 02.08.2023.
//

#ifndef ATOMIC_SHARED_POINTER_DECL_FWD_H
#define ATOMIC_SHARED_POINTER_DECL_FWD_H

#include "atomic_shared_pointer.h"
#include "hazard_pointer_domain.h"
#include "hazard_ptr_reclaimer.h"
#include "thread_entry_list.h"

namespace lu {
    using DefaultPolicy = detail::HazardPointersGenericPolicy<>;

    template <class Policy, class Allocator = std::allocator<std::byte>>
    using HazardPointers = detail::HazardPointerDomain<Policy, Allocator>;

    using DefaultReclaimer = HazardPtrReclaimer<DefaultPolicy>;

    template <class TValue, class Reclaimer = DefaultReclaimer>
    using AtomicSharedPtr = detail::AtomicSharedPtr<TValue, Reclaimer>;

    template <typename TValue>
    using SharedPtr = detail::SharedPtr<TValue>;

    template <typename TValue>
    using WeakPtr = detail::WeakPtr<TValue>;

    using detail::allocateShared;

    using detail::makeShared;
}

#endif //ATOMIC_SHARED_POINTER_DECL_FWD_H
