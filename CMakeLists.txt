cmake_minimum_required(VERSION 3.20)
project(atomic_shared_pointer)

set(CMAKE_CXX_STANDARD 20)

add_executable(atomic_shared_pointer
        main.cpp
        src/atomic_shared_pointer.h
        src/utils.h
        src/hazard_pointer_domain.h
        src/thread_entry_list.h
        src/hazard_ptr_reclaimer.h
)
