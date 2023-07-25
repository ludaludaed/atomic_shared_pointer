//
// Created by ludaludaed on 19.07.2023.
//

#ifndef ATOMIC_SHARED_POINTER_HAZARD_POINTER_H
#define ATOMIC_SHARED_POINTER_HAZARD_POINTER_H

namespace lu {
    class HazardPointerDomain {
    public:
        static constexpr size_t kMaxRetired = 1024;
        static constexpr size_t kHpMax = 4;
    private:

    };
}

#endif //ATOMIC_SHARED_POINTER_HAZARD_POINTER_H
