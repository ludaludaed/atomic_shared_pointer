#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <functional>
#include "vtyulb.h"
#include "std_atomic_sp.h"
#include "my_stack.h"

template <typename TContainer>
void stressTest(int actions, int threads) {
    std::vector<std::thread> workers;
    std::vector<std::vector<int>> generated(threads);
    std::vector<std::vector<int>> extracted(threads);
    TContainer container;
    std::atomic<int> initCount;
    for (int i = 0; i < threads; i++)
        workers.push_back(std::thread([i, actions, &container, &generated, &extracted, threads]() {
            for (int j = 0; j < actions / threads; j++) {
                if (rand() % 2) {
                    int a = rand();
                    container.push(a);
                    generated[i].push_back(a);
                } else {
                    auto a = container.pop();
                    if (a) {
                        extracted[i].push_back(*a);
                    }
                }
            }
        }));

    for (auto &thread: workers) {
        thread.join();
    }

    std::vector<int> all_generated;
    std::vector<int> all_extracted;
    for (int i = 0; i < generated.size(); i++) {
        for (int j = 0; j < generated[i].size(); j++) {
            all_generated.push_back(generated[i][j]);
        }
    }

    for (int i = 0; i < extracted.size(); i++) {
        for (int j = 0; j < extracted[i].size(); j++) {
            all_extracted.push_back(extracted[i][j]);
        }
    }

    while (true) {
        auto a = container.pop();
        if (a) {
            all_extracted.push_back(*a);
        } else {
            break;
        }
    }

    assert(all_generated.size() == all_extracted.size());

    std::sort(all_generated.begin(), all_generated.end());
    std::sort(all_extracted.begin(), all_extracted.end());
    for (int i = 0; i < all_extracted.size(); i++)
        assert(all_generated[i] == all_extracted[i]);
}

void abstractStressTest(const std::function<void(int, int)> &foo) {
    for (int i = 1; i <= std::thread::hardware_concurrency(); i++) {
        std::cout << "\t" << i;
    }
    std::cout << std::endl;
    for (int i = 500000; i <= 2000000; i += 500000) {
        std::cout << i << "\t";
        for (int j = 1; j <= std::thread::hardware_concurrency(); j++) {
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            foo(i, j);
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "\t";
        }
        std::cout << std::endl;
    }
}

void stacksCompare() {
    std::cout << "__________________________________Stack compare__________________________________" << std::endl;
    std::cout << std::endl << "from vtyulb:" << std::endl;
    abstractStressTest(stressTest<LFStructs::LFStack<int>>);
    std::cout << std::endl << "from std:" << std::endl;
    abstractStressTest(stressTest<std_atomic_sp::LockFreeStack<int>>);
    std::cout << std::endl << "from me:" << std::endl;
    abstractStressTest(stressTest<lu::LockFreeStack<int>>);
};

int main() {
    stacksCompare();
    return 0;
}
