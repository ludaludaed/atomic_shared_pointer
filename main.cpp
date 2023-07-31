#include <iostream>
#include <memory>
#include "include/atomic_shared_pointer.h"
#include "include/thread_entry_list.h"
#include <thread>
#include <vector>


int main() {
    using namespace lu;
    SharedPtr<int> sp = makeShared<int>(10);
    std::cout << *sp;
    return 0;
}
