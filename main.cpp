#include <iostream>
#include <memory>
#include "include/atomic_shared_pointer.h"
#include <thread>
#include <vector>

class TestDeleter {
public:
    TestDeleter() = default;

    TestDeleter(const TestDeleter &deleter) {
        std::cout << "copy" << std::endl;
    }

    TestDeleter(TestDeleter &&deleter) {
        std::cout << "move" << std::endl;
    }

    template <class T>
    void operator()(T *del) const {
        delete del;
    }
};

int main() {

    lu::SharedPtr<int> sp;
    TestDeleter deleter;
    sp.reset(new int(), std::move(deleter));
    return 0;
}
