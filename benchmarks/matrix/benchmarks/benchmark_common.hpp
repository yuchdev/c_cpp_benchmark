#pragma once
// Shared benchmark configuration constants for C++ benchmarks
#include <cstddef>

namespace bench {

constexpr int WARMUP_ITERS  = 3;
constexpr int MEASURE_ITERS = 20;

// Matrix sizes used in both C and C++ benchmarks
constexpr std::size_t SIZES[] = {32, 128, 512};

// Fixed sizes used for compile-time C++ benchmarks
constexpr int FIXED_SIZES[] = {3, 4, 8, 16};

} // namespace bench
