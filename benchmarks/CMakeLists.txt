cmake_minimum_required(VERSION 3.16)
project(dvel_benchmark)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
include(RustCore)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE dvel_core dl pthread)
