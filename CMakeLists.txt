cmake_minimum_required(VERSION 3.16)
project(c_cpp_benchmarks C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BUILD_GENERIC_BENCHMARKS "Build generic C/C++ benchmarks" ON)
option(BUILD_MATRIX_BENCHMARKS "Build matrix C/C++ benchmarks" ON)

if(BUILD_GENERIC_BENCHMARKS)
  add_subdirectory(generic_perf_compare)
endif()

if(BUILD_MATRIX_BENCHMARKS)
  enable_testing()
  add_subdirectory(matrix_perf_compare)
endif()
