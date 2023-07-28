cmake_minimum_required(VERSION 3.20)
project(atomic_shared_pointer)

set(CMAKE_CXX_STANDARD 20)

add_executable(atomic_shared_pointer
        main.cpp
        include/atomic_shared_pointer.h
        include/utils.h
        include/hazard_pointer.h
        include/thread_entry_list.h
        )
