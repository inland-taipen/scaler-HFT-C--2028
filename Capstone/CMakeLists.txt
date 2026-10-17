cmake_minimum_required(VERSION 3.14)
project(CapstoneOrderBook CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(main_ob)
include_directories(bench)

# Main benchmark executable
add_executable(bench bench/bench.cpp)

# Test executables
add_executable(tick_test decision_tests/tick_test.cpp)
add_executable(workload_test decision_tests/workload_test.cpp)