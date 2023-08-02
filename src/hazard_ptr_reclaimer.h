//
// Created by ludaludaed on 01.08.2023.
//

#ifndef ATOMIC_SHARED_POINTER_HAZARD_PTR_RECLAIMER_H
#define ATOMIC_SHARED_POINTER_HAZARD_PTR_RECLAIMER_H

#include "hazard_pointer_domain.h"
#include "atomic_shared_pointer.h"

namespace lu {
    template <class Policy, class Allocator = std::allocator<std::byte>>
    class HazardPtrReclaimer {
        using Domain = detail::HazardPointerDomain<Policy, Allocator>;

    public:
        template <class TValue>
        using GuardedPtr = typename Domain::template GuardedPtr<TValue>;

        template <class TValue>
        static GuardedPtr<TValue> protect(const std::atomic<TValue *> &ptr) {
            return reclaimer.protect(ptr);
        }

        static void delayDecrementRef(detail::ControlBlockBase *control_block) {
            struct Disposer {
                void operator()(detail::ControlBlockBase *control_block) const {
                    control_block->decrementRef();
                }
            };
            reclaimer.template retire<Disposer>(control_block);
        }

        static void delayDecrementWeakRef(detail::ControlBlockBase *control_block) {
            struct Disposer {
                void operator()(detail::ControlBlockBase *control_block) const {
                    control_block->decrementWeakRef();
                }
            };
            reclaimer.template retire<Disposer>(control_block);
        }

    private:
        static inline Domain &reclaimer = Domain::instance();
    };
}

#endif //ATOMIC_SHARED_POINTER_HAZARD_PTR_RECLAIMER_H
