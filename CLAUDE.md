# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

A reproducible C vs C++ micro-benchmark suite. Every benchmark is a **pair** of programs — one in
C, one in C++ — that perform identical logical work, so that any measured speed difference comes
from language/compiler capability rather than algorithm differences. Two independent suites:

- `benchmarks/generic/` — five paired benchmarks (groups `a`–`e`), each isolating one specific
  compiler lever: `qsort`+fn-pointer vs `std::sort`+lambda (a), function-pointer callback vs
  template/lambda dispatch (b), C struct-of-fn-pointers vtable vs inline C++ class operators (c),
  Array-of-Structs vs Structure-of-Arrays layout (d), runtime S-box table vs `constexpr`
  compile-time-fused table (e).
- `benchmarks/matrix/` — hand-written C matrix routines vs Eigen (C++), across dynamic (`MatrixXd`)
  and fixed-size (`Matrix<double,N,N>`) Eigen matrices, over configurable sizes and ops (transpose,
  add, mul, matvec, …).

This repo also has `AGENTS.md` (root) and `.github/copilot-instructions.md`, which contain the same
project rules for other AI tools — keep all three in sync if you change conventions or invariants
documented here.

## Build

Requires CMake 3.16+, a C11 compiler, a C++17 compiler (same toolchain for both), Python 3.7+.
Eigen3 3.3+ is auto-fetched via `FetchContent` if not found on the system.

```bash
# macOS
brew install cmake eigen python
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j$(sysctl -n hw.logicalcpu)
```

Always build `Release` — Debug builds make benchmark numbers meaningless.

Useful CMake options:
```bash
-DBUILD_GENERIC_BENCHMARKS=OFF   # skip the generic suite
-DBUILD_MATRIX_BENCHMARKS=OFF    # skip the matrix suite
-DMATRIX_CPP_AGGRESSIVE=OFF      # flag-equal comparison: put C++ at -O2 too (default ON = -O3 -march=native)
```

On Windows, the C benchmarks call POSIX `clock_gettime(CLOCK_MONOTONIC, …)` — build under
MSYS2/MinGW-w64 (UCRT64 shell, `-G Ninja`) or WSL2. Plain MSVC is not supported.

## Test

```bash
# C/C++ correctness + CLI-parser + Eigen cross-check tests
ctest --test-dir cmake-build --output-on-failure

# Python unit tests for the plotting script
python3 -m unittest tests.test_plot_results -v

# Full-pipeline integration test: builds+runs both suites end-to-end and asserts
# C++ actually wins (>=95% of paired comparisons, every generic group's average
# speedup > 1.0, and the compute-bound matrix ops win at every size)
python3 -m unittest tests.test_benchmark_results -v
```

Run a single CTest case: `ctest --test-dir cmake-build -R c_tests --output-on-failure` (registered
names: `c_tests`, `cpp_tests`, `options_tests`).

There is no linter configured; don't add one unless asked. `.clang-format` and `.cmake-format`
define style only (see Conventions below) — there's no CI step enforcing them yet.

## Running benchmarks

```bash
# Full run, both suites, writes CSV/JSON to the given output dir
python3 scripts/run_all_benchmarks.py --build-dir cmake-build --output-dir benchmark-results --repeats 5

# Plot an existing results dir without re-running anything
python3 scripts/run_all_benchmarks.py --plot-only benchmark-results

# Everything: both suites at representative sizes, every matrix op across a
# 4..512 dynamic size sweep, best-of-5 repeats, plus plots, one command
python3 scripts/run_all_benchmarks.py --build-dir cmake-build --output-dir benchmark-results --all

# Individual binaries
./cmake-build/benchmarks/generic/a_std_sort_cpp 1000000
./cmake-build/benchmarks/matrix/c_matrix_bench --help
```

`run_all_benchmarks.py` orchestrates configure → build → run → write results; `compile_report.py`
turns a results dir into a Markdown report; `plot_results.py` (needs `pip install matplotlib`)
renders per-operation PNGs plus an `overall_speedup.png` and `summary.md`. Outputs: `runs.csv`
(long-format per-run rows), `summary.csv` (mean/min/max/stdev), `runs.json`, `results_{group}.csv`.

## Architecture notes

- **`benchmarks/matrix/benchmarks/bench_options.h`** and **`bench_report.h`** are shared headers
  included by *both* the C and C++ matrix drivers — they must stay valid C11 *and* C++17
  simultaneously (no C++-only syntax). This is what lets `c_matrix_bench` and `cpp_matrix_bench`
  expose an identical CLI (`--sizes`, `--ops`, `--warmup`, `--iters`, `--repeats`, `--format
  table|csv|json`, …).
- **`benchmarks/matrix/CMakeLists.txt`** deliberately splits optimization flags by target: the C
  side (`c_matrix` library, `c_matrix_bench`) is pinned to `-O2` as a portable baseline; the C++
  side (`cpp_matrix_bench`) gets `-O3 -march=native -funroll-loops -ffp-contract=fast` plus
  `EIGEN_NO_DEBUG` when `MATRIX_CPP_AGGRESSIVE=ON` (the default). This asymmetry is the point of
  the suite, not an oversight — don't "fix" it by equalizing flags.
- Generic benchmarks are self-contained single-file programs (no shared library); matrix
  benchmarks share a `c_matrix` static library between the benchmark binary and the C tests.
- `tests/test_benchmark_results.py` is an integration test, not a unit test: it actually builds and
  runs both suites and statistically asserts the project's core thesis (C++ wins). Expect it to be
  slow; it's the test to run when validating a change that could affect timing behavior itself.

## Benchmark invariants (apply to all benchmark code, both suites)

1. C and C++ implementations of a pair must do **identical logical work** on identical data.
2. Anti-optimization sink: read/assign the result through a `volatile` after the timed region, or
   the compiler will dead-code-eliminate the whole computation.
3. No I/O (printf, CSV writes) inside the timed region.
4. Fixed LCG seed constants `1664525` / `1013904223`, same seed in the C and C++ drivers — do not
   change without updating both.
5. Warmup iterations before measuring; report the **minimum** average across `--repeats` (suppress
   OS jitter, don't average away it).
6. Everything is single-threaded — never enable Eigen OpenMP or a thread pool.
7. Never add `virtual` dispatch to a benchmark hot path (defeats the comparison); never link Eigen
   to an external BLAS (OpenBLAS/MKL) in the benchmark targets (must use Eigen's own kernels).

## Conventions

- C11 / C++17, formatted per `.clang-format` (LLVM base, 4-space indent, Allman braces, left
  pointer alignment `int* p`, no column limit); `CMakeLists.txt` formatted per `.cmake-format`
  (4-space indent, 100-char width).
- A `volatile` global function pointer in a C benchmark (e.g. `static volatile op_t g_op = ...;`)
  is intentional anti-devirtualization, not a bug — don't refactor it away.
- Prefer lambdas/templates over function pointers in new C++ benchmark code; use `.noalias()` on
  Eigen GEMM assignments (`C.noalias() = A * B;`) to avoid a defensive temporary.

## Adding a new benchmark

- New generic pair: add `benchmarks/generic/f_<name>_c.c` + `f_<name>_cpp.cpp`, register both
  executables in `benchmarks/generic/CMakeLists.txt`, follow the `volatile`-sink pattern from
  `a_qsort_c.c`, document the lever it isolates in `docs/c_cpp_benchmarking.md`, and add its run
  parameters to `scripts/run_all_benchmarks.py`.
- New matrix op: implement in `benchmarks/matrix/src/c/ops.c` (+ declare in
  `include/c_matrix/matrix.h`), add the Eigen equivalent in `src/cpp/benchmarks_cpp.cpp`, register
  the op name in `benchmarks/bench_options.h`, add correctness tests in `tests/c/test_matrix.c` and
  `tests/cpp/test_eigen_ops.cpp`, then verify with `ctest --test-dir cmake-build`.
</content>
