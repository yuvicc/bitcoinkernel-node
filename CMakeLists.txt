cmake_minimum_required(VERSION 3.15)
project(bitcoinkernel_node CXX)

find_package(binary_p2p CONFIG REQUIRED)


add_executable(bitcoinkernel_node src/bitcoinkernel_node.cpp src/main.cpp)

target_link_libraries(bitcoinkernel_node PRIVATE binary_p2p::binary_p2p)


install(TARGETS bitcoinkernel_node DESTINATION "."
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        )
