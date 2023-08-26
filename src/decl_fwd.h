//
// Created by ludaludaed on 02.08.2023.
//

#ifndef ATOMIC_SHARED_POINTER_DECL_FWD_H
#define ATOMIC_SHARED_POINTER_DECL_FWD_H

#include "atomic_shared_pointer.h"
#include "hazard_pointer_domain.h"
#include "thread_entry_list.h"

namespace lu {
    template <size_t MaxHP = 4, size_t MaxRetired = 256, size_t ScanDelay = 8>
    using HPolicy = detail::HazardPointersGenericPolicy<MaxHP, MaxRetired, ScanDelay>;

    template <class Policy, class Allocator = std::allocator<std::byte>>
    using HazardPointers = detail::HazardPointerDomain<Policy, Allocator>;

    template <class TValue, class Reclaimer = HazardPointers<HPolicy<>>>
    using AtomicSharedPtr = detail::AtomicSharedPtr<TValue, Reclaimer>;

    template <class TValue, class Reclaimer = HazardPointers<HPolicy<>>>
    using AtomicWeakPtr = detail::AtomicWeakPtr<TValue, Reclaimer>;

    template <typename TValue>
    using SharedPtr = detail::SharedPtr<TValue>;

    template <typename TValue>
    using WeakPtr = detail::WeakPtr<TValue>;

    using detail::allocateShared;

    using detail::makeShared;
}// namespace lu

#endif//ATOMIC_SHARED_POINTER_DECL_FWD_H
