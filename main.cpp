#include <iostream>
#include <memory>
#include "include/atomic_shared_pointer.h"
#include "include/thread_local.h"
#include <thread>
#include <vector>

class Base {

};

class Derived : public Base{

};

int main() {
    {


        lu::SharedPtr<Derived> sp;

        lu::WeakPtr<Base> wp(sp);

    }
//    auto foo = [](const int *a) {
//        std::cout << *a << std::endl;
//    };
//
//    lu::ThreadLocalRep<int, decltype(foo)> rep(foo);
//    std::vector<std::thread> workers;
//    workers.reserve(100);
//    for (int i = 0; i < 100; ++i) {
//        workers.emplace_back([=]() {
//            lu::ThreadLocal<int> local;
//            local.get() = i;
//        });
//    }
//
//    for (auto &it: workers) {
//        if (it.joinable()) {
//            it.join();
//        }
//    }
    return 0;
}
