cmake_minimum_required(VERSION 3.5.1)
project(obsr C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(fmt)

add_library(obsr STATIC
        include/obsr.h
        include/obsr_except.h
        include/obsr_types.h

        src/obsr_internal.h
        src/obsr.cpp
        src/obsr_except.cpp
        src/util/handles.h
        src/storage/storage.h
        src/storage/storage.cpp
        src/storage/listener_storage.h
        src/storage/listener_storage.cpp
        src/instance.h
        src/instance.cpp
        src/util/time.h
        src/util/time.cpp
        src/io/serialize.h
        src/io/serialize.cpp
        src/os/socket.h
        src/os/socket.cpp
        src/internal_except.h
        src/os/io.h
        src/os/io.cpp
        src/io/buffer.h
        src/io/buffer.cpp
        src/net/io.h
        src/net/io.cpp
        src/io/nio.h
        src/io/nio.cpp
        src/debug.h
        src/debug.cpp
        src/util/state.h
        src/net/client.h
        src/net/client.cpp
        src/net/serialize.h
        src/net/serialize.cpp
        src/util/general.h
        src/net/server.h
        src/net/server.cpp)
target_include_directories(obsr
        PUBLIC ${INCLUDES}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
target_link_libraries(obsr PRIVATE Threads::Threads fmt::fmt)


add_executable(test test.cpp)
target_link_libraries(test obsr)

add_executable(test2 test2.cpp)
target_include_directories(test2
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test2 obsr)

add_executable(test3 test3.cpp)
target_include_directories(test3
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test3 obsr)
