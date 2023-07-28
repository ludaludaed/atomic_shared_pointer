#include <iostream>
#include <memory>
#include "include/atomic_shared_pointer.h"
#include "include/thread_entry_list.h"
#include <thread>
#include <vector>

int main() {
    lu::EntriesHolder<int> h;

    h.getValue() = 10;

    lu::EntriesHolder<int> h2;

    std::cout << h2.getValue();
    return 0;
}
