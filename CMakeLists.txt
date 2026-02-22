cmake_minimum_required(VERSION 3.20)
project(obsr
        LANGUAGES C CXX
        VERSION 1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

include(GNUInstallDirs)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(DEBUG=1)

        if (NOT DEFINED TRACE_LEVEL)
                set(TRACE_LEVEL 1)
        endif ()
        if (NOT DEFINED TRACE_SINK)
                if (UNIX)
                        set(TRACE_SINK 1)
                else ()
                        set(TRACE_SINK 0)
                endif ()
        endif ()

        add_compile_definitions(TRACE_LEVEL=${TRACE_LEVEL} TRACE_SINK=${TRACE_SINK})
endif ()

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(fmt REQUIRED)
find_package(looper REQUIRED)
find_package(Crow REQUIRED)
find_package(RapidJSON REQUIRED)

file(GLOB INCLUDE_FILES ${CMAKE_SOURCE_DIR}/include/*.h)

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
        src/internal_except.h
        src/io/buffer.h
        src/io/buffer.cpp
        src/net/io.h
        src/net/io.cpp
        src/debug.h
        src/debug.cpp
        src/util/state.h
        src/net/client.h
        src/net/client.cpp
        src/net/serialize.h
        src/net/serialize.cpp
        src/util/general.h
        src/net/server.h
        src/net/server.cpp
        src/util/bits.h
        src/obsr_types.cpp
        src/global.h
        src/global.cpp
        src/diagnostics/storage.h
        src/diagnostics/storage.cpp
        src/diagnostics/server.h
        src/diagnostics/server.cpp)
target_include_directories(obsr
        PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:include/obsr>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
target_link_libraries(obsr PUBLIC Threads::Threads fmt::fmt looper::looper Crow::Crow RapidJSON)

install(TARGETS obsr EXPORT obsrTargets
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${INCLUDE_FILES}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/obsr)
install(EXPORT obsrTargets
        FILE obsrTargets.cmake
        NAMESPACE obsr::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/obsr")

include(CMakePackageConfigHelpers)
configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/obsrConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/obsrConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/obsr
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/obsrConfig.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/obsr)