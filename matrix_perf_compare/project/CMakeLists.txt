# Common compile flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra)
endif()

# ---- Optimization knobs --------------------------------------------------
# The C side stays a *portable* hand-written baseline (-O2, no machine
# tuning).  The C++ side is *aggressively* optimized: -O3, native ISA
# (AVX2/FMA where available), loop unrolling and fast-math, plus full Eigen
# vectorisation.  This intentionally showcases how a modern optimized C++
# linear-algebra stack overwhelms straightforward portable C.
option(MATRIX_CPP_AGGRESSIVE "Aggressively optimize the C++ benchmark" ON)

set(C_BASELINE_FLAGS -O2)

if(MATRIX_CPP_AGGRESSIVE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(CPP_AGGRESSIVE_FLAGS -O3 -funroll-loops -fno-math-errno -ffp-contract=fast)
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-march=native" HAS_MARCH_NATIVE)
  if(HAS_MARCH_NATIVE)
    list(APPEND CPP_AGGRESSIVE_FLAGS -march=native -mtune=native)
  endif()
  set(CPP_AGGRESSIVE_DEFS EIGEN_NO_DEBUG)
else()
  set(CPP_AGGRESSIVE_FLAGS -O2)
  set(CPP_AGGRESSIVE_DEFS "")
endif()

# ---- Eigen: prefer system install, fall back to FetchContent ----
find_package(Eigen3 3.3 QUIET NO_MODULE)
if(NOT Eigen3_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    eigen
    URL      https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
  )
  set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
  set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(eigen)
endif()

# ---- Include directories ----
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set(BENCH_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/../benchmarks)

# ---- C static library ----
add_library(c_matrix STATIC
  ../src/c/matrix.c
  ../src/c/ops.c
)
target_include_directories(c_matrix PUBLIC ${INCLUDE_DIR})
target_compile_options(c_matrix PRIVATE ${C_BASELINE_FLAGS})

# ---- C benchmark executable (portable baseline) ----
add_executable(c_matrix_bench ../src/c/benchmarks_c.c)
target_link_libraries(c_matrix_bench PRIVATE c_matrix m)
target_include_directories(c_matrix_bench PRIVATE ${INCLUDE_DIR} ${BENCH_DIR})
target_compile_options(c_matrix_bench PRIVATE ${C_BASELINE_FLAGS})

# ---- C++ benchmark executable (aggressively optimized) ----
add_executable(cpp_matrix_bench ../src/cpp/benchmarks_cpp.cpp)
target_link_libraries(cpp_matrix_bench PRIVATE Eigen3::Eigen c_matrix)
target_include_directories(cpp_matrix_bench PRIVATE ${INCLUDE_DIR} ${BENCH_DIR})
target_compile_options(cpp_matrix_bench PRIVATE ${CPP_AGGRESSIVE_FLAGS})
target_compile_definitions(cpp_matrix_bench PRIVATE ${CPP_AGGRESSIVE_DEFS})

# ---- C tests ----
add_executable(c_matrix_tests ../tests/c/test_matrix.c)
target_link_libraries(c_matrix_tests PRIVATE c_matrix m)
target_include_directories(c_matrix_tests PRIVATE ${INCLUDE_DIR})

# ---- CLI options tests (shared header) ----
add_executable(bench_options_tests ../tests/c/test_options.c)
target_include_directories(bench_options_tests PRIVATE ${BENCH_DIR})

# ---- C++ tests ----
add_executable(cpp_matrix_tests ../tests/cpp/test_eigen_ops.cpp)
target_link_libraries(cpp_matrix_tests PRIVATE Eigen3::Eigen c_matrix m)
target_include_directories(cpp_matrix_tests PRIVATE ${INCLUDE_DIR})

# ---- CTest integration ----
enable_testing()
add_test(NAME c_tests       COMMAND c_matrix_tests)
add_test(NAME cpp_tests     COMMAND cpp_matrix_tests)
add_test(NAME options_tests COMMAND bench_options_tests)

# ---- Optional run_benchmarks target ----
add_custom_target(run_benchmarks
  COMMAND c_matrix_bench
  COMMAND cpp_matrix_bench
  DEPENDS c_matrix_bench cpp_matrix_bench
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
  COMMENT "Running all benchmarks"
)
