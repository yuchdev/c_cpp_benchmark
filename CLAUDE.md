# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

A reproducible C vs C++ micro-benchmark suite. Every benchmark is a **pair** of programs — one in
C, one in C++ — that perform identical logical work, so that any measured speed difference comes
from language/compiler capability rather than algorithm differences. Two independent suites:

- `benchmarks/generic/` — six paired benchmarks (groups `a`–`f`), each isolating one specific
  compiler lever: `qsort`+fn-pointer vs `std::sort`+lambda (a), function-pointer callback vs
  template/lambda dispatch (b), C struct-of-fn-pointers vtable vs inline C++ class operators (c),
  Array-of-Structs vs Structure-of-Arrays layout (d), runtime S-box table vs `constexpr`
  compile-time-fused table (e), runtime-sized FIR tap loop vs `template<N>` compile-time-unrolled
  kernel (f).
- `benchmarks/matrix/` — hand-written C matrix routines vs Eigen (C++), across dynamic (`MatrixXd`)
  and fixed-size (`Matrix<double,N,N>`) Eigen matrices, over configurable sizes and ops (transpose,
  add, mul, matvec, …). On top of that op × size grid sit seven paired **scenarios** (`chain`,
  `fixed`, `cliff`, `batch`, `block`, `tri`, `conv`): parameter sweeps that each isolate one reason
  for, or limit to, the C++ advantage (expression fusion, compile-time N = 2..16 against a C
  macro-unrolled baseline, cache cliff, in-place `Map` over C buffers, strided blocks,
  triangular/symmetric kernels, a 3×3 image blur). They are informational: some deliberately show C
  tying or winning. See `docs/matrix_benchmark_methodology.md` §8.

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

# Python unit tests: plotting, scenario charts, matrix row classification / option forwarding
python3 -m unittest tests.test_plot_results tests.test_plot_scenarios tests.test_run_all_benchmarks -v

# Full-pipeline integration test: builds+runs both suites end-to-end and asserts
# C++ actually wins (>=95% of paired core comparisons, every generic group's average
# speedup > 1.0, and the compute-bound matrix ops win at every size) and that every
# scenario produced its files/figures, covers its full sweep, and the structural claims
# that hold with a wide margin are true
python3 -m unittest tests.test_benchmark_results -v
```

Known flakiness: the "95% of paired comparisons" assertion occasionally fails on small
element-wise ops (`add`/`sub`/`scale`/`add3` at 32–128), where C and C++ genuinely tie because the
working set is memory/L2-bound. This predates the scenarios (it failed 2 of 3 runs on a pristine
checkout of the previous commit on the macOS dev machine); rerun before assuming a regression.

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

# Only some matrix scenarios (core,chain,fixed,cliff,batch,block,tri,conv), best-of-5
python3 scripts/run_all_benchmarks.py --skip-generic --matrix-scenarios chain,cliff --matrix-repeats 5 --plot

# Individual binaries
./cmake-build/benchmarks/generic/a_std_sort_cpp 1000000
./cmake-build/benchmarks/matrix/c_matrix_bench --help
```

`run_all_benchmarks.py` orchestrates configure → build → run → write results; `compile_report.py`
turns a results dir into a Markdown report; `plot_results.py` (needs `pip install matplotlib`)
renders per-operation PNGs plus an `overall_speedup.png` and `summary.md`; `plot_scenarios.py` (called by it)
adds the speedup heatmaps and one figure per matrix scenario, including the blurred images from `conv`. Outputs: `runs.csv`
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
- **`benchmarks/matrix/benchmarks/bench_scenarios.h`** is the third shared C11+C++17 header. It is the
  single source of truth for every scenario's sweep points, the exact CSV row names
  (`bench_scn_name()`: `<lang>_<scenario>_<variant>[_n<param>]_<rows>x<cols>`; the sweep parameter is
  always `rows`), the per-call work estimates that drive iteration scaling, and the `BENCH_ESCAPE` /
  `BENCH_CLOBBER` optimization barriers. `scripts/run_all_benchmarks.py::classify_matrix_row` parses
  those names, so changing a name or a scenario token means changing both sides and the tests.
- Scenario kernels exist twice: C in `src/c/scenarios.c` (+ `c_matrix/fixed.h`, instantiated per N from
  `fixed_impl.inc` — "templates by `#include`") and Eigen in `include/cpp_matrix/eigen_scenarios.hpp`.
  The C++ driver *and* `tests/cpp/test_eigen_ops.cpp` call the same Eigen functions, so what is timed
  is exactly what is cross-checked against C. Input data is generated by the C library and converted,
  so both languages start from bit-identical matrices.
- Scenario sweeps time cheap calls with scaled iteration counts (`bench_iters_scaled`): `--iters` is
  the count for large sizes. Do not use a fixed 20 iterations for tiny matrices — macOS
  `CLOCK_MONOTONIC` ticks at ~1 µs. (The core grid uses the plain `--iters`.)
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
8. Scenario timing loops wrap the timed call with `BENCH_ESCAPE(buffer)` (once) and `BENCH_CLOBBER()`
   (after every call), in *both* languages (`BENCH_MEASURE` macro in C, `measure()` in C++). Without
   them the optimizer may hoist loop-invariant tiny-matrix work out of the loop and the benchmark
   measures nothing. C scenario code uses that macro, not a function-pointer callback, so the call
   stays visible to the optimizer just like the C++ lambda.
9. A scenario's sweep points, iteration counts and row names come only from `bench_scenarios.h`.
   Never hard-code them in one driver.

## Conventions

- C11 / C++17, formatted per `.clang-format` (LLVM base, 4-space indent, Allman braces, left
  pointer alignment `int* p`, no column limit); `CMakeLists.txt` formatted per `.cmake-format`
  (4-space indent, 100-char width).
- A `volatile` global function pointer in a C benchmark (e.g. `static volatile op_t g_op = ...;`)
  is intentional anti-devirtualization, not a bug — don't refactor it away.
- Prefer lambdas/templates over function pointers in new C++ benchmark code; use `.noalias()` on
  Eigen GEMM assignments (`C.noalias() = A * B;`) to avoid a defensive temporary.

## Adding a new benchmark

- New generic pair: add `benchmarks/generic/g_<name>_c.c` + `g_<name>_cpp.cpp` (next free letter
  after `f`), register both executables in `benchmarks/generic/CMakeLists.txt`, follow the
  `volatile`-sink pattern from
  `a_qsort_c.c`, document the lever it isolates in `docs/c_cpp_benchmarking.md`, and add its run
  parameters to `scripts/run_all_benchmarks.py`.
- New matrix op: implement in `benchmarks/matrix/src/c/ops.c` (+ declare in
  `include/c_matrix/matrix.h`), add the Eigen equivalent in `src/cpp/benchmarks_cpp.cpp`, register
  the op name in `benchmarks/bench_options.h`, add correctness tests in `tests/c/test_matrix.c` and
  `tests/cpp/test_eigen_ops.cpp`, then verify with `ctest --test-dir cmake-build`. The op also feeds
  the `fixed` sweep: add it to `fixed_impl.inc`, `bench_fixed_n.inc`, `scn::Fixed` and `run_fixed_n`
  (or document why it is excluded).
- New scenario: follow `docs/matrix_benchmark_methodology.md` §8.4 (scenario bit + sweep + work
  estimate in the shared headers, C kernel + Eigen twin, a runner in each driver, the token in
  `MATRIX_SCENARIOS` and a `SweepSpec` in `scripts/plot_scenarios.py`, tests on every layer).
</content>
