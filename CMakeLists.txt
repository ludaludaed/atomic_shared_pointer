cmake_minimum_required(VERSION 3.20)
project(atomic_shared_pointer)

set(CMAKE_CXX_STANDARD 20)

add_executable(atomic_shared_pointer
        benchmarks/main.cpp
        benchmarks/vtyulb.h
        src/atomic_shared_pointer.h
        src/utils.h
        src/hazard_pointer_domain.h
        src/thread_entry_list.h
        src/decl_fwd.h
        benchmarks/std_atomic_sp.h
        structures/lock_free_stack.h
        structures/lock_free_queue.h)
