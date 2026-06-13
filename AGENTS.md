# AGENTS.md — Agentic Guidelines for `c_cpp_benchmark`

This file is the authoritative guide for AI coding agents (Codex, GitHub Copilot coding agent,
JetBrains Junie, and similar) working in this repository.

---

## Project Purpose

A reproducible collection of C vs C++ micro-benchmarks organized into two suites:

| Suite | Directory | What it measures |
|---|---|---|
| **Generic** | `generic_perf_compare/` | Five paired benchmarks: sorting, element-wise transform, vector math, data layout, compile-time evaluation |
| **Matrix** | `matrix_perf_compare/` | Matrix operations comparing hand-written C against Eigen (C++) at multiple sizes |

The goal is to demonstrate **structural** (not stylistic) performance advantages of C++ over C by
isolating a single compiler lever per test. C and C++ programs always perform identical logical
work — ratios above 1.0× mean C is slower.

---

## Repository Layout

```
c_cpp_benchmark/
├── generic_perf_compare/            # Generic benchmark suite (five groups a–e)
│   ├── benchmarks/                  # CMake subdirectory wiring the paired binaries
│   ├── a_qsort_c.c                  # Group A — C: qsort + function pointer
│   ├── a_std_sort_cpp.cpp           # Group A — C++: std::sort + lambda
│   ├── b_callback_c.c               # Group B — C: volatile callback pointer
│   ├── b_template_cpp.cpp           # Group B — C++: template lambda (SIMD)
│   ├── c_struct_api.c               # Group C — C: struct of function pointers (vtable)
│   ├── c_class_operator.cpp         # Group C — C++: inline class operators
│   ├── d_buffer_copy_c.c            # Group D — C: Array-of-Structs (AoS)
│   ├── d_buffer_move_cpp.cpp        # Group D — C++: Structure-of-Arrays (SoA)
│   ├── e_runtime_table_c.c          # Group E — C: runtime S-box (24 rounds/byte)
│   └── e_constexpr_table_cpp.cpp    # Group E — C++: constexpr fused table (1 lookup/byte)
│
├── matrix_perf_compare/             # Matrix benchmark suite
│   ├── benchmarks/
│   │   ├── bench_options.h          # Shared CLI parser (valid C11 and C++17)
│   │   └── bench_report.h           # Shared reporter: table / csv / json output
│   ├── include/
│   │   ├── c_matrix/matrix.h        # C matrix API (row-major, heap, portable)
│   │   └── cpp_matrix/eigen_ops.hpp # Eigen wrapper helpers
│   ├── src/c/                       # C matrix library + benchmark driver
│   │   ├── matrix.c / ops.c         # Core matrix routines
│   │   └── benchmarks_c.c           # CLI-driven C benchmark runner
│   ├── src/cpp/
│   │   └── benchmarks_cpp.cpp       # CLI-driven C++ (Eigen) benchmark runner
│   └── tests/
│       ├── c/test_matrix.c          # C correctness tests
│       ├── c/test_options.c         # CLI parser tests
│       └── cpp/test_eigen_ops.cpp   # Eigen cross-check tests
│
├── scripts/
│   ├── run_all_benchmarks.py        # Orchestrator: configure → build → run → write results
│   ├── compile_report.py            # Generate Markdown report from result files
│   └── plot_results.py              # Generate Matplotlib charts (PNG + summary.md)
│
├── tests/
│   └── test_plot_results.py         # Python unit tests for plot_results.py
│
├── docs/                            # Methodology and reference documentation
│   ├── c_cpp_benchmarking.md        # Narrative walkthrough of all five generic tests
│   ├── generic_benchmark_methodology.md  # Warmup / measurement / fairness rules
│   ├── matrix_benchmark_methodology.md   # Matrix suite architecture and results
│   ├── benchmark_visualization.md   # plot_results.py full reference
│   └── compiler_explorer.md         # Godbolt examples for assembly inspection
│
├── examples/                        # Usage examples
├── CMakeLists.txt                   # Root CMake (requires 3.16+, C11, C++17)
├── .clang-format                    # LLVM-based style: 4-space indent, Allman braces
└── .cmake-format                    # CMake style: 4-space indent, 100-char line width
```

---

## Build Requirements

| Dependency | Version | Notes |
|---|---|---|
| CMake | 3.16+ | Required on all platforms |
| C compiler | C11 | GCC or Clang (Linux/macOS); MinGW-w64 GCC on Windows |
| C++ compiler | C++17 | Same compiler as C |
| Eigen3 | 3.3+ | Auto-fetched via `FetchContent`; or install via package manager (see below) |
| Python | 3.7+ | Required for orchestration scripts and Python tests |
| Matplotlib | any recent | `pip install matplotlib` — only needed for `plot_results.py` |

> **Windows note**: the C benchmarks use `clock_gettime(CLOCK_MONOTONIC, …)`, a POSIX API.
> Build under MSYS2/MinGW-w64 or WSL2. Pure MSVC builds are not supported.

---

## Build Commands

### Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake python3 python3-pip libeigen3-dev
pip3 install matplotlib

cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j$(nproc)
```

### macOS

```bash
brew install cmake eigen python
pip3 install matplotlib

cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j$(sysctl -n hw.logicalcpu)
```

### Windows — MSYS2 / MinGW-w64 (recommended)

Open the **UCRT64** shell from [MSYS2](https://www.msys2.org):

```bash
pacman -S --needed \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-eigen3 \
    python python-pip
pip install matplotlib

cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build
```

Alternatively, open a WSL2 Ubuntu shell and use the Ubuntu instructions above.

### CMake Options (all platforms)

```bash
# Build a single suite only
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DBUILD_GENERIC_BENCHMARKS=OFF
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DBUILD_MATRIX_BENCHMARKS=OFF

# Disable aggressive C++ flags (flag-equal comparison: both sides at -O2)
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DMATRIX_CPP_AGGRESSIVE=OFF
```

---

## Test Commands

### CTest — C/C++ unit tests (correctness, CLI options, Eigen cross-checks)

```bash
ctest --test-dir cmake-build --output-on-failure
```

Three test executables are registered: `c_tests`, `cpp_tests`, `options_tests`.

### Python unit tests — plot_results.py

```bash
python3 -m unittest tests.test_plot_results -v
```

**Always run both CTest and Python tests before committing.** There are no linter invocations
defined in the project yet; do not add them unless explicitly requested.

---

## Running Benchmarks

```bash
# Full run — both suites, write CSV/JSON results to benchmark_results/
python3 scripts/run_all_benchmarks.py \
    --build-dir cmake-build \
    --output-dir benchmark_results \
    --repeats 5

# Generic only with representative workload sizes
python3 scripts/run_all_benchmarks.py \
    --skip-matrix \
    --build-dir cmake-build \
    --output-dir benchmark_results \
    --repeats 5 \
    --sort "10000,100000,1000000,10000000" \
    --callback 10000000 \
    --struct-api 10000000 \
    --buffer "1048576,x2,4" \
    --table 10000000

# Plot an existing results directory (no re-run)
python3 scripts/run_all_benchmarks.py --plot-only benchmark_results
```

---

## Coding Conventions

### C Sources (`*.c`)

- Standard: **C11** (`set(CMAKE_C_STANDARD 11)`)
- Formatting: `.clang-format` — LLVM-based, **4-space indent**, **Allman braces**, no column limit
- Pointer alignment: left (`int* p`)
- Use `volatile` global function-pointer variables to prevent the optimizer from devirtualizing
  callback benchmarks (this is intentional, not a bug)
- Timer: `CLOCK_MONOTONIC` via `clock_gettime`
- Avoid VLAs, `alloca`, and compiler extensions in shared headers

### C++ Sources (`*.cpp`, `*.hpp`)

- Standard: **C++17** (`set(CMAKE_CXX_STANDARD 17)`)
- Same clang-format style as C
- Prefer **lambdas** and **templates** over function pointers for configurable operations
- Timer: `std::chrono::steady_clock`
- Eigen: use `.noalias()` for GEMM assignments; prefer `Eigen::Matrix<double, N, N>` (fixed-size)
  for N ≤ 16
- Use `constexpr` for compile-time tables, seeds, and constants
- Prefer SoA (Structure-of-Arrays) over AoS (Array-of-Structs) in hot-loop data structures
- Do **not** use `virtual` functions in benchmark classes — it defeats the benchmarks' purpose

### CMake (`CMakeLists.txt`)

- Format with cmake-format (`.cmake-format`): 4-space indent, 100-char line width
- Never change the flag split: C baseline stays at `-O2`; C++ aggressive flags are controlled by
  `MATRIX_CPP_AGGRESSIVE` (default ON) using `target_compile_options`
- Use `target_*` commands; avoid `add_compile_options` for optimization flags
- New test executables must be registered with `add_test`

---

## Benchmark Methodology — Invariants

**These invariants must be preserved in all benchmark code. Violating them invalidates results.**

1. **Same logical work.** C and C++ programs must implement identical algorithms on identical data.
   Performance differences come from language/compiler differences only.
2. **Anti-optimization sinks.** After each timed region, read an output element through a `volatile`
   pointer or assign to a `volatile` variable. This prevents the compiler from treating the entire
   computation as dead code.
3. **No I/O inside the timed region.** CSV writes and `printf` calls happen outside the measured
   loop.
4. **Same random initialization.** Use LCG constants `1664525` / `1013904223` and the same seed in
   both C (`matrix_fill_rand`) and C++ (`fill_rand`). Do not change these constants.
5. **Warmup before measurement.** Run `--warmup` iterations (default 3) before the timed loop to
   warm the CPU caches and branch predictor.
6. **Best-of-repeats.** Run the measurement `--repeats` times; keep the **minimum** average to
   suppress OS scheduling jitter.
7. **Single-threaded.** All benchmarks are single-threaded. Do not enable Eigen OpenMP or thread
   pools.

---

## Adding a New Generic Benchmark (Group `f`, etc.)

1. Create paired source files in `generic_perf_compare/`:
   `f_<description>_c.c` and `f_<description>_cpp.cpp`
2. Add both executables to `generic_perf_compare/benchmarks/CMakeLists.txt`.
3. Follow the anti-optimization sink pattern from existing sources (see `a_qsort_c.c`).
4. Document the test in `docs/c_cpp_benchmarking.md` — explain what compiler lever it isolates.
5. Add run parameters and expected group label to `scripts/run_all_benchmarks.py`.

## Adding a New Matrix Operation

1. Add the C implementation to `matrix_perf_compare/src/c/ops.c` and declare it in
   `include/c_matrix/matrix.h`.
2. Add the Eigen equivalent to `matrix_perf_compare/src/cpp/benchmarks_cpp.cpp`.
3. Register the operation name in the list inside `benchmarks/bench_options.h`.
4. Add a correctness test in `tests/c/test_matrix.c` and `tests/cpp/test_eigen_ops.cpp`.
5. Run `ctest --test-dir cmake-build --output-on-failure` to verify.

---

## Output Format Reference

`run_all_benchmarks.py` writes to `--output-dir`:

| File | Description |
|---|---|
| `runs.csv` | Long-format per-run timing table (`suite,group,benchmark,language,run,measure,metric,value,unit`) |
| `summary.csv` | Grouped statistics (mean, min, max, stdev) |
| `runs.json` | Averaged results in JSON format |
| `results_{group}.csv` | Per-group CSV files (e.g., `results_a.csv`) |

`plot_results.py` outputs:
- One `<operation>.png` per operation
- `overall_speedup.png` — bar chart with one bar per operation
- `summary.md` — textual summary with average/best/worst speedup and winner

---

## What NOT to Do

- Do not add `printf` or file writes inside a timed benchmark loop.
- Do not hard-code matrix sizes; they are set via `--sizes` CLI argument.
- Do not enable `virtual` dispatch in C++ benchmark hot paths.
- Do not use `alloca` or VLAs inside benchmark functions.
- Do not change the LCG seed constants (`1664525` / `1013904223`) without updating both drivers.
- Do not run benchmarks in `Debug` build; results are meaningless without optimization.
- Do not link Eigen to an external BLAS backend (OpenBLAS, MKL) in the benchmark targets — the
  comparison must use Eigen's built-in kernels.
- Do not add `--no-pager` or similar shell helpers to CMakeLists; CI must be portable.
