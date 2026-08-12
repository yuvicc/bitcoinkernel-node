cmake_minimum_required(VERSION 3.22)
project(bitcoinkernel_node CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(binary_p2p CONFIG REQUIRED)

set(BITCOINKERNEL_ROOT "/usr/local" CACHE PATH "Prefix where libbitcoinkernel is installed")
option(BITCOINKERNEL_STATIC "Link libbitcoinkernel statically" ON)

find_path(BITCOINKERNEL_INCLUDE_DIR
    NAMES bitcoinkernel.h
    HINTS "${BITCOINKERNEL_ROOT}/include"
)

if(BITCOINKERNEL_STATIC)
    find_library(BITCOINKERNEL_LIBRARY
        NAMES libbitcoinkernel.a
        HINTS "${BITCOINKERNEL_ROOT}/lib"
    )
else()
    find_library(BITCOINKERNEL_LIBRARY
        NAMES bitcoinkernel
        HINTS "${BITCOINKERNEL_ROOT}/lib"
    )
endif()

if(NOT BITCOINKERNEL_INCLUDE_DIR OR NOT BITCOINKERNEL_LIBRARY)
    message(FATAL_ERROR
        "libbitcoinkernel not found under ${BITCOINKERNEL_ROOT}. "
        "Build and install Bitcoin Core's kernel library, or pass "
        "-DBITCOINKERNEL_ROOT=<prefix>.")
endif()

message(STATUS "libbitcoinkernel: ${BITCOINKERNEL_LIBRARY}")

add_library(bitcoinkernel::bitcoinkernel UNKNOWN IMPORTED)
set_target_properties(bitcoinkernel::bitcoinkernel PROPERTIES
    IMPORTED_LOCATION "${BITCOINKERNEL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${BITCOINKERNEL_INCLUDE_DIR}"
)

find_package(Threads REQUIRED)

add_executable(bitcoinkernel_node
    src/main.cpp
)

target_include_directories(bitcoinkernel_node PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(bitcoinkernel_node PRIVATE
    binary_p2p::binary_p2p
    bitcoinkernel::bitcoinkernel
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

install(TARGETS bitcoinkernel_node RUNTIME DESTINATION bin)
