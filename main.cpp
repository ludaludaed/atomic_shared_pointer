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
    int *ptr = new int(10);
    HazardPointerDomain<GenericPolicy<>>::instance().retire<Dispose>(ptr);
    std::atomic<int*> atom{ptr};
    HazardPointerDomain<GenericPolicy<>>::instance().protect(atom);
    return 0;
}
