//
// Created by ludaludaed on 02.08.2023.
//

#ifndef ATOMIC_SHARED_POINTER_FWD_H
#define ATOMIC_SHARED_POINTER_FWD_H

#include "atomic_shared_pointer.h"
#include "hazard_pointer_domain.h"
#include "hazard_ptr_reclaimer.h"

namespace lu {
    template <class TValue>
    using AtomicSharedPtrHP = AtomicSharedPtr<TValue, HazardPtrReclaimer<GenericPolicy<>>>;
}

#endif //ATOMIC_SHARED_POINTER_FWD_H
