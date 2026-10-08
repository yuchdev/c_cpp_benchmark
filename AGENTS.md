# AGENTS.md — Agentic Guidelines for `c_cpp_benchmark`

This file is the authoritative guide for AI coding agents (Codex, GitHub Copilot coding agent,
JetBrains Junie, and similar) working in this repository.

---

## Project Purpose

A reproducible collection of C vs C++ micro-benchmarks organized into two suites:

| Suite | Directory | What it measures |
|---|---|---|
| **Generic** | `benchmarks/generic/` | Six paired benchmarks: sorting, element-wise transform, vector math, data layout, compile-time evaluation, compile-time kernel unrolling |
| **Matrix** | `benchmarks/matrix/` | Matrix operations comparing hand-written C against Eigen (C++) at multiple sizes, plus seven paired **scenarios** (`chain`, `fixed`, `cliff`, `batch`, `block`, `tri`, `conv`): sweeps that each isolate one reason for, or limit to, the C++ advantage. Scenarios are informational — some deliberately show C tying or winning |

The goal is to demonstrate **structural** (not stylistic) performance advantages of C++ over C by
isolating a single compiler lever per test. C and C++ programs always perform identical logical
work — ratios above 1.0× mean C is slower.

---

## Repository Layout

```
c_cpp_benchmark/
├── benchmarks/
│   ├── generic/                     # Generic benchmark suite (six groups a–f)
│   └── matrix/                      # Matrix benchmark suite
├── scripts/                         # Benchmark orchestrators
├── tests/
├── docs/                            # Methodology and reference documentation
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

# Everything: both suites at representative sizes, every matrix op across a
# 4..512 dynamic size sweep, every matrix scenario (the compile-time-size sweep
# always covers N = 2..16), best-of-5 repeats throughout, plus plots — one command
python3 scripts/run_all_benchmarks.py --build-dir cmake-build --output-dir benchmark_results --all

# Only some matrix scenarios (core,chain,fixed,cliff,batch,block,tri,conv)
python3 scripts/run_all_benchmarks.py --skip-generic --matrix-scenarios chain,cliff --matrix-repeats 5 --plot
```

`--all` cannot be combined with `--skip-generic`/`--skip-matrix`; any of `--sort`, `--callback`,
`--struct-api`, `--buffer`, `--table`, `--fir`, `--matrix-sizes`, `--matrix-ops`,
`--matrix-scenarios`, `--matrix-repeats` passed alongside it overrides just that one default.

---

## Testing

```bash
# C/C++ correctness, Eigen cross-checks (every scenario kernel vs its C twin), CLI parser
ctest --test-dir cmake-build --output-on-failure

# Python unit tests: plotting, scenario charts, matrix row classification / option forwarding
python3 -m unittest tests.test_plot_results tests.test_plot_scenarios tests.test_run_all_benchmarks -v

# Full-pipeline integration test (slow: runs both suites end to end)
python3 -m unittest tests.test_benchmark_results -v
```

The integration test's "95 % of paired core comparisons" rule can fail intermittently on small
element-wise ops (`add`/`sub`/`scale`/`add3` at 32–128), where C and C++ genuinely tie; this predates
the scenarios, so rerun before assuming a regression. Scenario results are asserted only where the
C++ win held with a wide margin; cases where C wins or ties are documented, not asserted.

---

## Coding Conventions

### C Sources (`*.c`)

- Standard: **C11** (`set(CMAKE_C_STANDARD 11)`)
- Formatting: `.clang-format` — LLVM-based, **4-space indent**, **Allman braces**, no column limit
- Pointer alignment: left (`int* p`)
- Use `volatile` global function-pointer variables to prevent the optimizer from devirtualizing
  callback benchmarks (this is intentional, not a bug)
- Timer: `CLOCK_MONOTONIC` via `clock_gettime`
- Avoid VLAs, `alloca`, and compiler extensions in shared headers (the one exception is the
  `#if defined(__GNUC__)`-guarded `BENCH_ESCAPE`/`BENCH_CLOBBER` barrier in `bench_scenarios.h`,
  which has a portable fallback)

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
8. **Barriers around cheap kernels.** Scenario timing loops call `BENCH_ESCAPE(buffer)` once and
   `BENCH_CLOBBER()` after every timed call, in both languages (`BENCH_MEASURE` in C, `measure()` in
   C++). Otherwise the optimizer can hoist loop-invariant tiny-matrix work out of the loop and the
   benchmark measures nothing. C scenario code uses that macro, not a function-pointer callback.
9. **Iterations scale with cost.** `--iters` is the count for large sizes; cheap calls get more
   (`bench_iters_scaled`, `bench_iters_for_core`) so a timed sample spans well over the ~1 µs clock
   resolution of macOS. Do not hard-code 20 iterations for tiny matrices.
10. **One source of truth for scenarios.** Sweep points, row names (`bench_scn_name()`) and work
    estimates live only in `benchmarks/matrix/benchmarks/bench_scenarios.h`; both drivers include it.
    `scripts/run_all_benchmarks.py::classify_matrix_row` parses those names.

---

## Adding a New Generic Benchmark (Group `g`, etc. — next free letter after `f`)

1. Create paired source files in `benchmarks/generic/`:
   `g_<description>_c.c` and `g_<description>_cpp.cpp`
2. Add both executables to `benchmarks/generic/CMakeLists.txt`.
3. Follow the anti-optimization sink pattern from existing sources (see `a_qsort_c.c`).
4. Document the test in `docs/c_cpp_benchmarking.md` — explain what compiler lever it isolates.
5. Add run parameters and expected group label to `scripts/run_all_benchmarks.py`.

## Adding a New Matrix Operation

1. Add the C implementation to `benchmarks/matrix/src/c/ops.c` and declare it in
   `benchmarks/matrix/include/c_matrix/matrix.h`.
2. Add the Eigen equivalent to `benchmarks/matrix/src/cpp/benchmarks_cpp.cpp`.
3. Register the operation name in the list inside `benchmarks/matrix/benchmarks/bench_options.h`.
4. Add a correctness test in `tests/c/test_matrix.c` and `tests/cpp/test_eigen_ops.cpp`.
5. The op also feeds the compile-time-size sweep: add it to `include/c_matrix/fixed_impl.inc`,
   `src/c/bench_fixed_n.inc`, `scn::Fixed` in `include/cpp_matrix/eigen_scenarios.hpp` and
   `run_fixed_n` in `src/cpp/bench_scenarios_cpp.cpp` (or document why it is excluded).
6. Run `ctest --test-dir cmake-build --output-on-failure` to verify.

## Adding a New Matrix Scenario

Full checklist: `docs/matrix_benchmark_methodology.md` §8.4. In short: scenario bit and name in
`bench_options.h`; sweep constants and a `bench_work_*` estimate in `bench_scenarios.h`; C kernel in
`src/c/scenarios.c` and its Eigen twin in `include/cpp_matrix/eigen_scenarios.hpp`; a runner in
`bench_scenarios_c.c` and `bench_scenarios_cpp.cpp`; the scenario token in `MATRIX_SCENARIOS`
(`scripts/run_all_benchmarks.py`) and a `SweepSpec` in `scripts/plot_scenarios.py`; tests at every
layer (C kernel, C-vs-Eigen cross-check, options/naming, Python, integration pair count).

---

## Output Format Reference

`run_all_benchmarks.py` writes to `--output-dir`:

| File | Description |
|---|---|
| `runs.csv` | Long-format per-run timing table (`suite,group,benchmark,language,run,measure,metric,value,unit`) |
| `summary.csv` | Grouped statistics (mean, min, max, stdev) |
| `runs.json` | Averaged results in JSON format |
| `results_{group}.csv` | Per-group CSV files (e.g., `results_a.csv`; each matrix scenario has its own, e.g. `results_matrix_chain.csv`) |
| `matrix_raw/` | Raw per-language matrix CSVs; `matrix_raw/images/` holds the `conv` scenario's PGM images |

`plot_results.py` outputs:
- One `<operation>.png` per core operation (scenario operations are namespaced `matrix_<scenario>/<variant>` and get no per-operation chart)
- `overall_speedup.png` — bar chart with one bar per core operation
- `summary.md` — textual summary with average/best/worst speedup and winner, plus a `# Scenario Results` section
- `speedup_heatmap.png`, `scenario_overview.png`, `scenario_{chain,fixed,cliff,batch,block,tri,conv}.png`,
  `chain_heatmap.png`, `fixed_size_heatmap.png`, `conv_images.png` (from `plot_scenarios.py`;
  green = C++ faster, purple = C faster)

---

## What NOT to Do

- Do not add `printf` or file writes inside a timed benchmark loop.
- Do not hard-code matrix sizes; they are set via `--sizes` CLI argument. (Scenario sweeps are the
  exception: they are defined once, in `bench_scenarios.h`, and shared by both drivers.)
- Do not time tiny kernels without `BENCH_ESCAPE`/`BENCH_CLOBBER` and scaled iteration counts.
- Do not enable `virtual` dispatch in C++ benchmark hot paths.
- Do not use `alloca` or VLAs inside benchmark functions.
- Do not change the LCG seed constants (`1664525` / `1013904223`) without updating both drivers.
- Do not run benchmarks in `Debug` build; results are meaningless without optimization.
- Do not link Eigen to an external BLAS backend (OpenBLAS, MKL) in the benchmark targets — the
  comparison must use Eigen's built-in kernels.
- Do not add `--no-pager` or similar shell helpers to CMakeLists; CI must be portable.
