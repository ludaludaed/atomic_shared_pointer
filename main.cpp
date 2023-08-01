#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include "include/hazard_pointer.h"
#include "include/atomic_shared_pointer.h"
#include "include/thread_entry_list.h"


using namespace lu;

class Dispose {
public:
    template <class TValue>
    void operator()(TValue *v) {
        delete v;
    }
};

int main() {
    HazardPointerDomain<GenericPolicy<>> &inst = HazardPointerDomain<GenericPolicy<>>::instance();
    int *ptr = new int(10);
    std::atomic<int *> atom{ptr};
    auto first = inst.protect(atom);
    inst.retire<Dispose>(ptr);

    return 0;
}
