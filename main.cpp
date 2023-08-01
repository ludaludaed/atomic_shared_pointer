#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include "src/hazard_pointer_domain.h"
#include "src/atomic_shared_pointer.h"
#include "src/thread_entry_list.h"


using namespace lu;

class Dispose {
public:
    template <class TValue>
    void operator()(TValue *v) {
        std::cout << "d";
        delete v;
    }
};

int main() {
    HazardPointerDomain<> &inst = HazardPointerDomain<>::instance();
    int *ptr = new int(10);
    std::atomic<int *> atom{ptr};
    auto first = inst.protect(atom);
    inst.retire<Dispose>(ptr);

    return 0;
}
