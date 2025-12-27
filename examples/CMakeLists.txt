cmake_minimum_required(VERSION 3.16)
project(dvel_examples)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
include(RustCore)

add_executable(ffi_minimal ffi_minimal.cpp)
target_include_directories(ffi_minimal PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../include")
target_link_libraries(ffi_minimal PRIVATE dvel_core dl pthread)
