cmake_minimum_required(VERSION 3.5.1)
project(obsr C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(obsr STATIC
        include/obsr.h
        include/obsr_except.h
        include/obsr_types.h
        src/obsr_internal.h

        src/obsr.cpp
        src/os/mutex.h
        src/os/mutex.cpp
        src/obsr_except.cpp
        src/util/handles.h
        src/storage/storage.h
        src/storage/storage.cpp)
target_include_directories(obsr
        PUBLIC ${INCLUDES}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
target_link_libraries(obsr PRIVATE Threads::Threads)


add_executable(test test.cpp)
target_link_libraries(test obsr)
