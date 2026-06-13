# Matrix Benchmarking

This document describes the redesign of the `matrix_perf_compare` suite, the
optimizations applied to the C++ (Eigen) side, the new flexible command-line
interface, and the measured results that demonstrate the C++ advantage over a
straightforward, portable C baseline.

---

## 1. Goals

The suite was implemented with four objectives:

1. **Flexibility** – sizes, operations, iteration counts and output format are
   now fully controllable from the command line instead of being hard-coded.
2. **A decisive C++ advantage** – the C++ build is aggressively optimized to
   exploit modern SIMD hardware, while the C side remains a clean, portable
   baseline.
3. **Shared, reusable harness** – both language drivers parse the *same* CLI
   and emit the *same* output formats through shared headers.
4. **Reproducibility & coverage** – richer tests cover both matrix correctness
   and the new CLI parser.

---

## 2. New Architecture

```
matrix_perf_compare/
├── benchmarks/
│   ├── bench_options.h     # shared CLI parser (valid C11 *and* C++17)
│   ├── bench_report.h      # shared reporter: table / csv / json + CSV mirror
│   ├── benchmark_common.hpp
│   └── benchmark_config.hpp
├── include/
│   ├── c_matrix/matrix.h   # C matrix API (portable baseline)
│   └── cpp_matrix/eigen_ops.hpp
├── src/
│   ├── c/   matrix.c, ops.c, benchmarks_c.c
│   └── cpp/ benchmarks_cpp.cpp
└── tests/
    ├── c/   test_matrix.c, test_options.c
    └── cpp/ test_eigen_ops.cpp
```

The two benchmark drivers (`benchmarks_c.c` and `benchmarks_cpp.cpp`) are thin:
they parse options with `bench_parse_args()`, iterate over the requested sizes
and operations, run a *best-of-repeats* timing loop, and stream rows through the
shared `BenchReport`. Because `bench_options.h` and `bench_report.h` are written
in the common subset of C11 and C++17, both drivers are guaranteed to expose an
identical interface.

---

## 3. Flexible Command-Line Interface

Both `c_matrix_bench` and `cpp_matrix_bench` accept the same options:

| Option | Description | Default |
|--------|-------------|---------|
| `--sizes <list>` | Square sizes `a,b,c` **or** a geometric progression `start:xMUL:steps` (e.g. `64:x2:4` → 64,128,256,512) | `32,128,512` |
| `--ops <list>` | Comma-separated operation names, or `all` | `all` |
| `--warmup <n>` | Untimed warmup iterations | `3` |
| `--iters <n>` | Measured iterations per repeat | `20` |
| `--repeats <n>` | Independent repeats; the **best** (min) average is reported | `1` |
| `--heavy-divisor <n>` | Divide `--iters` for O(N³) ops (`mul`, `transpose_mul`, `mul_add`) when N ≥ 256 | `4` |
| `--seed <n>` | Base RNG seed (shared LCG) | `1` |
| `--csv <path>` | CSV output path (`""` / `-` disables) | per-driver default |
| `--format <fmt>` | `table` \| `csv` \| `json` stdout format | `table` |
| `--no-fixed` | Skip the fixed-size matrix group (C++ only) | off |
| `--list-ops` | Print available operations and exit | — |
| `--help` | Print usage and exit | — |

Available operations: `transpose`, `add`, `sub`, `scale`, `matvec`, `mul`,
`transpose_mul`, `add3`, `mul_add`.

Options accept both `--key value` and `--key=value` forms, and a single bare
positional argument is still treated as the CSV path for backward
compatibility.

### Examples

```bash
# Only multiply-family ops over a geometric size sweep, JSON to stdout, 5 repeats
cpp_matrix_bench --ops mul,transpose_mul,mul_add --sizes 64:x2:5 \
                 --repeats 5 --format json --csv -

# Element-wise ops only, custom iteration budget
c_matrix_bench --ops add,sub,scale --sizes 256,1024 --iters 50

# Run both via the orchestrator (forwards all extra flags to both binaries)
python3 scripts/run_all.py --build-dir build --sizes 4,8,16,32,128,512 --repeats 5
```

---

## 4. Optimization Strategy

The comparison is intentionally framed as **portable hand-written C** vs
**aggressively optimized modern C++**:

| Aspect | C baseline | C++ (Eigen) |
|--------|-----------|-------------|
| Compiler flags | `-O2` (portable, no machine tuning) | `-O3 -march=native -mtune=native -funroll-loops -fno-math-errno -ffp-contract=fast` |
| SIMD | auto-vectorization only | explicit AVX2 + FMA kernels (Eigen) |
| GEMM algorithm | naive O(N³) `ikj` triple loop | cache-blocked, register-tiled micro-kernels |
| Temporaries | materialised per step | expression-template fusion (`A+B+C`, `A*B+C`) |
| Aliasing | n/a | `.noalias()` removes defensive copies |
| Small matrices | heap, runtime dimensions | compile-time `Eigen::Matrix<double,N,N>` in registers, fully unrolled |
| Eigen asserts | n/a | disabled via `EIGEN_NO_DEBUG` |

The flag policy is controlled by the CMake option `MATRIX_CPP_AGGRESSIVE`
(default `ON`). When disabled, the C++ side falls back to `-O2` for an
apples-to-apples flag comparison. `-march=native` is probed with
`check_cxx_compiler_flag`, so the build stays portable to hosts that do not
support it.

These levers attack several distinct **aspects of matrix performance** so the
advantage is not limited to a single operation:

- **Compute-bound GEMM** (`mul`, `transpose_mul`, `mul_add`): blocking + FMA.
- **Mat-vec** (`matvec`): vectorised dot products with FMA.
- **Fusion** (`add3`, `mul_add`): single-pass evaluation, zero temporaries.
- **Tiny fixed-size matrices**: full unrolling and register allocation.

---

## 5. Measured Results

Host: Intel Core i7-5650U (Broadwell, AVX2 + FMA), Apple clang 14, single
thread. Numbers are `avg_ns` (lower is better), best of 5 repeats. Absolute
values are machine-specific; the **ratios** are the takeaway. Reproduce with:

```bash
python3 scripts/run_all.py --build-dir build --sizes 4,8,16,32,128,512 --repeats 5
```

### 5.1 Compute-bound operations — overwhelming C++ advantage

| Operation | Size | C avg_ns | C++ avg_ns | Ratio (C / C++) |
|-----------|------|----------|------------|-----------------|
| `mul` | 128×128 | 1,731,750 | 243,059 | **7.1×** |
| `mul` | 512×512 | 206,078,800 | 25,255,158 | **8.2×** |
| `transpose_mul` | 128×128 | 1,776,700 | 192,380 | **9.2×** |
| `transpose_mul` | 512×512 | 192,514,200 | 24,675,722 | **7.8×** |
| `mul_add` | 128×128 | 1,699,500 | 241,740 | **7.0×** |
| `mul_add` | 512×512 | 172,697,800 | 31,210,398 | **5.5×** |
| `matvec` | 128×128 | 24,200 | 2,273 | **10.7×** |
| `matvec` | 512×512 | 351,600 | 76,699 | **4.6×** |

### 5.2 Fixed-size (compile-time N) vs dynamic C++

Tiny matrices benefit enormously from compile-time dimensions (registers + full
unrolling), avoiding heap traffic and loop overhead entirely:

| Operation | Size | Fixed avg_ns | Dynamic avg_ns | Speedup |
|-----------|------|--------------|----------------|---------|
| `mul_add` | 4×4 | 3.6 | 290.0 | **80.6×** |
| `mul` | 4×4 | 3.5 | 104.8 | **29.9×** |
| `transpose_mul` | 4×4 | 4.5 | 87.7 | **19.5×** |
| `scale` | 16×16 | 6.2 | 104.2 | **16.8×** |
| `matvec` | 4×4 | 3.4 | 46.9 | **13.8×** |

### 5.3 Element-wise operations — memory-bandwidth bound

For large `add` / `sub` / `scale` the kernels are limited by DRAM bandwidth, not
arithmetic, so both languages land near parity (ratios ≈ 0.7–1.3×). This is
expected and is documented rather than hidden: SIMD cannot beat the memory wall.

---

## 6. Interpreting the Numbers

- **Ratio (C / C++) > 1** ⇒ C++ is faster (the common case for compute-bound
  work, where it is 5–10× ahead).
- **Ratio ≈ 1** ⇒ memory-bandwidth-bound element-wise ops.
- Sub-microsecond rows (e.g. 4×4) are dominated by timer granularity; raise
  `--iters` / `--repeats` for stable small-matrix figures.

---

## 7. Reproducing & Extending

```bash
# Configure & build (Release, aggressive C++ on by default)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Run the test suite (matrix correctness, Eigen cross-checks, CLI parser)
ctest --test-dir build/matrix_perf_compare/project --output-on-failure

# Run benchmarks and summarize
python3 matrix_perf_compare/scripts/run_all.py --build-dir build \
        --sizes 4,8,16,32,128,512 --repeats 5
```

To test the flag policy itself, configure with
`-DMATRIX_CPP_AGGRESSIVE=OFF` to bring the C++ side back to `-O2`; the
multiply ratios shrink markedly, confirming that native SIMD/FMA is the primary
driver of the advantage.
