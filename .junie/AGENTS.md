# AGENTS.md — Junie Guidelines for `c_cpp_benchmark`

> For the full agentic reference see the root [`AGENTS.md`](../AGENTS.md).
> This file contains the essential subset needed by JetBrains AI / Junie.

---

## Project at a Glance

C vs C++ micro-benchmark collection. Two suites:

- **`generic_perf_compare/`** — five paired benchmarks (a–e): sorting, element-wise transform,
  vec4 math, memory layout, compile-time evaluation
- **`matrix_perf_compare/`** — matrix operations comparing hand-written C vs Eigen at configurable
  sizes

## Build & Test (run from repository root)

```bash
# Build
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j4

# C/C++ tests (matrix correctness, CLI options, Eigen cross-checks)
ctest --test-dir cmake-build --output-on-failure

# Python tests
python3 -m unittest tests.test_plot_results -v
```

## Code Style

- C: **C11**, 4-space indent, Allman braces, left pointer alignment (`int* p`) — see `.clang-format`
- C++: **C++17**, same clang-format settings
- CMake: 4-space indent, 100-char line width — see `.cmake-format`

## Critical Invariants — Never Violate

1. C and C++ benchmark programs must perform **identical logical work**.
2. Place a **`volatile` read of an output element** after every timed region (anti-optimization sink).
3. **No I/O inside the timed region.**
4. Use LCG constants **`1664525` / `1013904223`** (same seed in C and C++ drivers).
5. Warmup before measuring; keep the **minimum average** across `--repeats` runs.
6. Benchmarks are **single-threaded** — do not enable Eigen OpenMP.

## Common Pitfalls

- Do not add `virtual` to benchmark hot-path methods.
- Do not run benchmarks in `Debug` mode.
- Do not link Eigen to OpenBLAS or MKL in benchmark targets.
- Do not change LCG seed constants without updating both drivers.
