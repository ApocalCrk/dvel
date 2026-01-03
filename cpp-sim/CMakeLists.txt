cmake_minimum_required(VERSION 3.16)
project(dvel_cpp_sim)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake")
include(RustCore)

set(DVEL_SIM_EXECUTABLES
  sim_baseline
  sim_scenario
  sim_sybil
  sim_scheduler
  sim_metrics
  gov_ledger
)

foreach(name IN LISTS DVEL_SIM_EXECUTABLES)
  add_executable(${name} "executables/${name}.cpp")
  target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/core")
  target_link_libraries(${name} PRIVATE dvel_core dl pthread)
endforeach()
