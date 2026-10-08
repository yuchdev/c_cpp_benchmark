# Matrix Benchmarking

This document describes the redesign of the `benchmarks/matrix` suite, the
optimizations applied to the C++ (Eigen) side, the new flexible command-line
interface, and the measured results that demonstrate the C++ advantage over a
straightforward, portable C baseline. Section 8 adds seven *scenarios*:
parameter sweeps that each isolate one reason for (or limit to) that advantage.

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
benchmarks/matrix/
├── benchmarks/
│   ├── bench_options.h     # shared CLI parser (valid C11 *and* C++17)
│   ├── bench_report.h      # shared reporter: table / csv / json + CSV mirror
│   ├── bench_scenarios.h   # scenario sweeps, row names, work estimates, barriers (C11 + C++17)
│   ├── benchmark_common.hpp
│   └── benchmark_config.hpp
├── include/
│   ├── c_matrix/matrix.h           # C matrix API (portable baseline)
│   ├── c_matrix/fixed.h            # compile-time-N kernels, N = 2..16 (+ fixed_impl.inc)
│   ├── cpp_matrix/eigen_ops.hpp
│   └── cpp_matrix/eigen_scenarios.hpp  # Eigen twins of the scenario kernels
├── src/
│   ├── c/   matrix.c, ops.c, scenarios.c, benchmarks_c.c,
│   │        bench_scenarios_c.{h,c}, bench_fixed_n.inc
│   └── cpp/ benchmarks_cpp.cpp, bench_scenarios_cpp.{hpp,cpp}
└── tests/
    ├── c/   test_matrix.c, test_options.c
    └── cpp/ test_eigen_ops.cpp
```

The two benchmark drivers (`benchmarks_c.c` and `benchmarks_cpp.cpp`) are thin:
they parse options with `bench_parse_args()`, iterate over the requested sizes
and operations, run a *best-of-repeats* timing loop, and stream rows through the
shared `BenchReport`. Because `bench_options.h` and `bench_report.h` are written
in the common subset of C11 and C++17, both drivers are guaranteed to expose an
identical interface. `benchmarks_c.c` / `benchmarks_cpp.cpp` hold the **core** grid (§3–§6);
`bench_scenarios_c.c` / `bench_scenarios_cpp.cpp` hold the scenarios (§8), which draw
their sweep points, row names and iteration counts from the shared `bench_scenarios.h`
so that the two languages cannot drift apart.

---

## 3. Flexible Command-Line Interface

Both `c_matrix_bench` and `cpp_matrix_bench` accept the same options:

| Option | Description | Default |
|--------|-------------|---------|
| `--sizes <list>` | Square sizes `a,b,c` **or** a geometric progression `start:xMUL:steps` (e.g. `64:x2:4` → 64,128,256,512) | `32,128,512` |
| `--ops <list>` | Comma-separated operation names, or `all` | `all` |
| `--warmup <n>` | Untimed warmup iterations | `3` |
| `--iters <n>` | Measured iterations per repeat (the scenario sweeps multiply it for cheap calls, see below) | `20` |
| `--repeats <n>` | Independent repeats; the **best** (min) average is reported | `1` |
| `--heavy-divisor <n>` | Divide `--iters` for O(N³) ops (`mul`, `transpose_mul`, `mul_add`) when N ≥ 256 | `4` |
| `--seed <n>` | Base RNG seed (shared LCG) | `1` |
| `--csv <path>` | CSV output path (`""` / `-` disables) | per-driver default |
| `--format <fmt>` | `table` \| `csv` \| `json` stdout format | `table` |
| `--scenarios <list>` | Comma-separated scenarios `core,chain,fixed,cliff,batch,block,tri,conv`, or `all` | `all` |
| `--no-fixed` | Skip the compile-time-size sweep (same as dropping `fixed` from `--scenarios`, whatever the option order) | off |
| `--image-dir <path>` | Existing directory for the `conv` scenario's PGM images | none |
| `--list-ops` | Print available operations and exit | — |
| `--list-scenarios` | Print available scenarios and exit | — |
| `--help` | Print usage and exit | — |

Available operations: `transpose`, `add`, `sub`, `scale`, `matvec`, `mul`,
`transpose_mul`, `add3`, `mul_add`.

`--sizes` shapes the **core** grid only; `--ops` filters the core grid, the `fixed` sweep and the
`cliff` sweep. The other scenarios have fixed sweeps defined once in `bench_scenarios.h`.

**Iteration scaling (scenarios).** A call that takes a few hundred nanoseconds cannot be timed with
20 iterations: `clock_gettime(CLOCK_MONOTONIC)` ticks at only ~1 µs on macOS, so such a sample is
mostly quantisation noise. The scenario sweeps therefore multiply `--iters` by up to 65536×
(`bench_iters_scaled()` in `bench_options.h`, driven by a rough work estimate per call) so every
timed sample spans well over the clock resolution; the `iterations` column of the CSV records what
was actually run. The core grid uses the plain `--iters` count.

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

# Only two scenarios, best-of-5
cpp_matrix_bench --scenarios chain,cliff --repeats 5

# Run both via the orchestrator (the --matrix-* flags are forwarded to both binaries)
python3 scripts/run_all_benchmarks.py --build-dir build --skip-generic \
        --matrix-sizes 4,8,16,32,128,512 --matrix-repeats 5 --plot
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
python3 scripts/run_all_benchmarks.py --build-dir build --skip-generic \
        --matrix-sizes 4,8,16,32,128,512 --matrix-repeats 5
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

> These figures come from the earlier C++-only fixed group (3/4/8/16), measured with a fixed
> 20 iterations. That group has been replaced by the `fixed` scenario (§8), which sweeps every
> N = 2..16, adds a compile-time-N **C** counterpart, scales iterations for tiny kernels and guards
> the timed loop with optimization barriers. Compare against §8.1, not against this table.

| Operation | Size | Fixed avg_ns | Dynamic avg_ns | Speedup |
|-----------|------|--------------|----------------|---------|
| `mul_add` | 4×4 | 3.6 | 290.0 | **80.6×** |
| `mul` | 4×4 | 3.5 | 104.8 | **29.9×** |
| `transpose_mul` | 4×4 | 4.5 | 87.7 | **19.5×** |
| `scale` | 16×16 | 6.2 | 104.2 | **16.8×** |
| `matvec` | 4×4 | 3.4 | 46.9 | **13.8×** |

### 5.3 Element-wise operations — memory-bandwidth bound

For large `add` / `sub` / `scale` / `add3` the kernels are limited by DRAM
bandwidth, not arithmetic, so both languages land near parity (ratios ≈ 0.7–1.3×).
This is expected and is documented rather than hidden: SIMD cannot beat the
memory wall. Large transpose operations are also sensitive to Eigen's
column-major versus C's row-major layout, so they are not included in the
integration test's overall C++-superiority percentage at sizes of 256 and above.

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
ctest --test-dir cmake-build --output-on-failure

# Run benchmarks and summarize
python3 scripts/run_all_benchmarks.py --build-dir cmake-build --skip-generic \
        --matrix-sizes 4,8,16,32,128,512 --matrix-repeats 5 --plot
```

To test the flag policy itself, configure with
`-DMATRIX_CPP_AGGRESSIVE=OFF` to bring the C++ side back to `-O2`; the
multiply ratios shrink markedly, confirming that native SIMD/FMA is the primary
driver of the advantage.

---

## 8. Scenarios

The core grid answers *how fast is each operation at each size?* The scenarios answer *which
specific lever produces the difference, and where does it stop?* Each one is a parameter sweep with
a C and a C++ implementation of identical logical work on identical data (the C library generates
the input and the C++ side converts it). Every scenario writes its rows into its own results group
`matrix_<scenario>` and gets its own figure. Select them with `--scenarios`; the default is all.

| Scenario | Question | C | C++ (Eigen) | Sweep |
|---|---|---|---|---|
| `chain` | Do expression templates really remove the temporaries? | k-1 binary `matrix_add` passes through two ping-pong temporaries | one fused expression `(A1 + … + Ak)`, built with a fold over `std::index_sequence` | k = 2..16 × N = 32…512 |
| `fixed` | What does a compile-time size buy? | `FixedMatN` kernels instantiated from `fixed_impl.inc`: literal loop bounds, `-O2` | `Matrix<double,N,N>`, `-O3 -march=native` | N = 2..16, all 9 ops |
| `cliff` | What happens when the working set leaves the cache? | `matrix_add` / `transpose` / `matvec` | the same expressions | N = 16..2048 (2 KB..96 MB) |
| `batch` | SIMD in place on caller-owned data | scalar 4×4 · 4-vector per point | `Map<Matrix<double,4,Dynamic>>`, `Y = T*X` over the same buffer | M = 1K..1M points |
| `block` | Strided submatrices | row-wise `memcpy`; i-k-j multiply using the leading dimension | `block()` copy; `block().noalias() = a.block() * b.block()` | bs = 4..256 inside 512×512 |
| `tri` | Skipping the half that does not matter | forward substitution; row dot products for lower(A·Aᵀ) | `triangularView<Lower>().solveInPlace`, `selfadjointView<Lower>().rankUpdate` | n = 16..256 |
| `conv` | A real image workload | 3×3 loops | nine shifted `block()` views fused into one expression | N = 64..1024 |

**Not like-for-like by construction.** Only the *work* is identical, not the toolchain: the C side
is `-O2`, the C++ side `-O3 -march=native -ffp-contract=fast` (§4). Where a scenario names a C++
"lever" it is the combination of that lever and the flags, exactly as in the core suite.

**Optimization barriers.** Scenario timing loops wrap every call in `BENCH_ESCAPE` (before) and
`BENCH_CLOBBER` (after each call) from `bench_scenarios.h`: empty `asm volatile` statements with a
memory clobber, the same trick as Google Benchmark's `DoNotOptimize`/`ClobberMemory`. Without them
an optimizer may legally collapse a loop over loop-invariant 2×2 matrices into almost nothing.

### 8.1 Measured results

Same host as §5 (Core i7-5650U, 4 MB L3), best of 5, geometric mean of `C / C++` over each sweep
(> 1 means C++ is faster). Absolute values are machine-specific; run
`python3 scripts/run_all_benchmarks.py --skip-generic --matrix-repeats 5 --plot` to regenerate them
along with `summary.md`.

| Scenario / variant | Geomean | Best point | Worst point |
|---|---:|---|---|
| `chain` N = 32 … 512 | 2.2× – 3.3× | 6.7× (k = 13, N = 256) | 1.05× (k = 2, N = 64) |
| `fixed` add / sub / add3 | 2.1× – 2.2× | 8.2× (N = 12) | 0.86× (add3, N = 3) |
| `fixed` mul / mul_add | 1.6× | 9.4× (N = 4) | 0.52× (mul, N = 7) |
| `fixed` transpose | 0.97× | 2.9× (N = 2) | 0.59× (N = 8) |
| `cliff` matvec | 3.6× | 6.8× (N = 64) | 1.4× (N = 2048, DRAM) |
| `cliff` transpose / add | 1.8× / 1.2× | 3.0× / 1.7× (N = 512) | 0.97× / 0.85× |
| `batch` xform4 | 1.4× | 2.0× (1K points) | 0.96× (16K points) |
| `block` mul | 5.3× | 12.2× (bs = 32) | 1.3× (bs = 4) |
| `block` copy | 1.02× | 1.15× (bs = 32) | 0.78× (bs = 256) |
| `tri` syrk | 6.3× | 13.2× (n = 256) | 1.8× (n = 16) |
| `tri` trsm | 1.4× | 3.2× (n = 256) | 0.57× (n = 16) |
| `conv` blur3x3 | **0.60×** | 0.75× (N = 1024) | 0.50× (N = 64) |

### 8.2 What the numbers say

- **Fusion pays off for every chain longer than two** (`chain`): one fused pass beats k-1 passes at
  every measured matrix size once k ≥ 3, by 2–3× on average. At k = 2 there is only one add to
  fuse, so the ratio is close to 1 (1.05–1.4×).
- **The advantage is a cache effect as much as a language effect** (`cliff`, `batch`): `matvec`
  is several times faster in C++ while the matrix is cache-resident (up to 6.8× here) and shrinks
  to ~1.4× once it streams from DRAM; `add` is close to a tie everywhere because it is
  bandwidth-bound.
- **Compile-time sizes are not a uniform win** (`fixed`): element-wise ops are consistently about
  2× faster, `mul` at N = 4 is 9×, but the ratio moves up and down with N (`mul` is 0.5× at N = 7
  and `transpose` is a tie), so read the heatmap rather than the average.
- **Blocked GEMM and the rank-k update are where C++ is far ahead** (`block` mul, `tri` syrk).
- **C holds its own, and sometimes wins** (reported, not hidden): strided block *copy* is a tie
  (row-wise `memcpy` is hard to beat), the triangular solve loses at small n, and the 3×3
  convolution written idiomatically in Eigen (nine fused `block()` views) is 1.3–2× *slower* than
  the plain C loop at every size. We have not diagnosed the convolution result; short columns
  with unaligned packet loads and scalar head/tail handling are the obvious suspects, but that is a
  hypothesis, not a measurement.
- **Which lever is it?** Rebuilding with `-DMATRIX_CPP_AGGRESSIVE=OFF` (both sides at `-O2`, one
  repeat, same host) separates language from flags: the `chain` advantage survives at 2.4×
  geometric mean, so it comes from the single fused pass, not from `-march=native`; the `fixed`
  advantage drops from 1.7× (all 135 points, best of 5) to 1.3× (one repeat), so part of it was the
  flags; and the `conv` gap widens to 0.36×.
- The `conv` scenario also checks correctness visibly: `conv_images.png` shows the input, both
  outputs and their difference (0 of 64,516 pixels differ on the reference host).

### 8.3 Figures produced

`speedup_heatmap.png` (core op × size), `scenario_overview.png` (geomean per variant),
`scenario_{chain,fixed,cliff,batch,block,tri,conv}.png`, `chain_heatmap.png`,
`fixed_size_heatmap.png`, `conv_images.png`. Colours follow one convention throughout: green means
C++ is faster, purple means C is faster, speedup is always `C time / C++ time`. See
[benchmark_visualization.md](benchmark_visualization.md).

### 8.4 Adding a scenario

1. `bench_options.h`: add the `BENCH_SCN_*` bit and the name in `BENCH_SCENARIO_NAMES`.
2. `bench_scenarios.h`: add the sweep constants and a `bench_work_*` estimate (it drives iteration
   scaling for *both* drivers).
3. C kernel in `src/c/scenarios.c` (declare it in `matrix.h`); Eigen twin in
   `eigen_scenarios.hpp`.
4. A runner in `bench_scenarios_c.c` (wrap the call in `BENCH_MEASURE`) and in
   `bench_scenarios_cpp.cpp` (`measure(...)`); name every row with `bench_scn_name()` and call
   `BENCH_ESCAPE` on the buffers.
5. `scripts/run_all_benchmarks.py`: add the scenario token to `MATRIX_SCENARIOS`;
   `scripts/plot_scenarios.py`: add a `SweepSpec` (or a custom figure) and a `SCENARIO_TITLES` entry.
6. Tests: a C kernel test, a C-vs-Eigen cross-check, naming/sweep checks in `test_options.c`, a
   Python test, and the expected pair count in `tests/test_benchmark_results.py`.
