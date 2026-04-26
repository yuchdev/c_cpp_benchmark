# Benchmark Methodology

## Overview

This document describes how the C vs C++ matrix benchmark is designed, what it measures, and how to interpret the results fairly.

---

## 1. Benchmark Design Principles

The benchmark compares hand-written C matrix routines against Eigen (a widely-used C++ linear algebra library) across three axes:

| Axis | C | C++ (Eigen Dynamic) | C++ (Eigen Fixed) |
|------|---|----------------------|--------------------|
| Matrix size known at... | Runtime | Runtime | Compile time |
| Memory layout | Row-major heap | Column-major heap | Stack / SIMD registers |
| Vectorisation | Auto (compiler) | Manual (SIMD intrinsics) | Fully unrolled + SIMD |
| Expression fusion | None | Partial (noalias) | Full |

---

## 2. Warmup and Measurement Strategy

Each benchmark follows this sequence:

1. **Setup** - allocate and fill matrices with the same deterministic LCG values.
2. **Warmup** - run the operation 3 times (configurable via `WARMUP_ITERS`). This brings data into CPU caches and exercises the branch predictor, so the first measurement is not penalised by cold-start effects.
3. **Measurement** - run the operation `MEASURE_ITERS` (default 20) times and record wall-clock time using `CLOCK_MONOTONIC` (C) or `std::chrono::high_resolution_clock` (C++).
4. **Average** - divide total elapsed nanoseconds by iteration count to get `avg_ns`.

Matrix multiplication at sizes ≥ 256 uses only `iters / 4` measurement iterations because even a single 512×512 multiply takes tens of milliseconds.

---

## 3. Fairness Rules

To ensure a fair comparison:

- **Same element type** - all matrices use `double` (64-bit IEEE 754 floating point).
- **Same matrix dimensions** - both C and C++ dynamic benchmarks run at N = 32, 128, 512.
- **Same initialisation** - the same linear-congruential generator (LCG constants `1664525` / `1013904223`) is used in both C (`matrix_fill_rand`) and C++ (`fill_rand`), seeded identically.
- **Same optimisation level** - both are compiled with `-O2`.
- **No I/O inside the timed region** - CSV writing and `printf` happen outside the measured loop.
- **Anti-optimisation sinks** - after each benchmark a `volatile` read of an output element prevents the compiler from eliminating the entire computation as dead code.

---

## 4. Why Fixed-size C++ Is Expected to Outperform C

When the matrix dimensions are compile-time constants (`Eigen::Matrix<double, N, N>`), the compiler can:

- **Fully unroll** inner loops (no loop-counter overhead).
- **Allocate matrices on the stack** or in SIMD registers (no heap round-trips, no `calloc`/`free`).
- **Emit optimal SIMD code** (SSE/AVX) for exactly the known size, including specialised 4×4 or 8×8 kernels.
- **Inline everything** into a single translation unit with no function-call overhead.

The C implementation allocates matrices on the heap and must handle arbitrary sizes at runtime, so none of these optimisations are available even with `-O2`.

---

## 5. Why Dynamic C++ Is a More Apples-to-Apples Comparison

`Eigen::MatrixXd` (dynamic) also allocates on the heap and stores dimensions at runtime, matching the C structure more closely.  The key remaining differences are:

- **Memory layout** - Eigen uses column-major by default; the C implementation uses row-major.  This affects cache performance differently per operation (transpose vs. matvec).
- **SIMD** - Eigen uses explicit SIMD intrinsics (SSE2/AVX at minimum); the C code relies entirely on auto-vectorisation.
- **Expression templates** - Eigen can fuse `A + B + C` or `A * B + C` into a single traversal, avoiding intermediate heap allocations.

The dynamic comparison is therefore the most relevant for understanding library-level overhead.

---

## 6. Result Interpretation Guidance

| Ratio (C / C++) | Interpretation |
|-----------------|----------------|
| > 1.0x          | C is **slower** than C++ for this operation |
| ≈ 1.0x          | Roughly equivalent |
| < 1.0x          | C is **faster** (uncommon; may indicate a measurement artefact) |

For small matrices (32×32), results can be noisy because individual runs are sub-microsecond. Interpret with caution.

For matrix multiply at 512×512, the difference between an O(N³) naive loop (C) and Eigen's blocked/SIMD kernel is expected to be large (often 5–20×).

---

## 7. Limitations

- **Single-threaded only** - all benchmarks are single-threaded. Eigen can use OpenMP or its own parallelism for large matrices, but that is disabled here to keep the comparison fair.
- **No BLAS backend** - Eigen is compiled without an external BLAS (e.g., OpenBLAS, MKL). Linking Eigen to BLAS would dramatically increase multiply performance.
- **Micro-benchmark caveats** - real application performance depends on memory access patterns, branch prediction, and surrounding code. Micro-benchmarks measure best-case scenario.
- **Column-major vs. row-major** - the layout mismatch means transpose performance will differ in directions that are not purely a C-vs-C++ question.
- **OS scheduling jitter** - on a shared system, individual measurements can vary ±10%. Run multiple times and compare averages.
