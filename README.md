# C/C++ Benchmarks Collection

This repository contains a comprehensive set of C/C++ benchmarks split into two families:

1.  **Generic programming-pattern benchmarks** (`generic_perf_compare`)
2.  **Matrix-operation benchmarks** (`matrix_perf_compare`)

---

## Quick Start

Get results for all benchmarks with a single command (requires Python 3 and CMake):

```bash
python3 scripts/run_all_benchmarks.py --build-dir build --output-dir benchmark_results
```

The script will automatically configure (via CMake), build, and execute all benchmarks, saving results to the `benchmark_results` directory.

---

## 1. Generic Benchmarks (`generic_perf_compare`)

These are paired micro-benchmarks comparing equivalent C and C++ implementations of common programming patterns.

| Group | C source              | C++ source                  | Focus                                                       |
|:------|:----------------------|:----------------------------|:------------------------------------------------------------|
| **a** | `a_qsort_c.c`         | `a_std_sort_cpp.cpp`        | `qsort` callback dispatch vs `std::sort` + lambda           |
| **b** | `b_callback_c.c`      | `b_template_cpp.cpp`        | Function-pointer callbacks vs template/lambda transforms    |
| **c** | `c_struct_api.c`      | `c_class_operator.cpp`      | C struct-style API vs C++ class operators/methods           |
| **d** | `d_buffer_copy_c.c`   | `d_buffer_move_cpp.cpp`     | Cache locality: C Array-of-Structs vs C++ Structure-of-Arrays traversal |
| **e** | `e_runtime_table_c.c` | `e_constexpr_table_cpp.cpp` | Runtime multi-round S-box transform vs compile-time `constexpr` table fusion |

---

## 2. Matrix Benchmarks (`matrix_perf_compare`)

This suite compares handwritten C matrix routines against [Eigen](https://eigen.tuxfamily.org/) (a modern C++ linear algebra library) across a range of matrix sizes and operations.

### Scope

| Category           | What is benchmarked                                                                                         |
|:-------------------|:------------------------------------------------------------------------------------------------------------|
| **C (runtime)**    | Hand-written row-major `double` matrices; naive O(N³) multiply; `calloc`/`free` lifecycle                   |
| **C++ Dynamic**    | `Eigen::MatrixXd` - heap-allocated, column-major, SIMD-optimised, expression-template fusion                |
| **C++ Fixed-size** | `Eigen::Matrix<double, N, N>` - stack/register allocated, fully unrolled, maximum compile-time optimisation |

*   **Operations**: `transpose`, `add`, `sub`, `scale`, `matvec`, `mul`, `transpose_mul`, `add3`, `mul_add`.
*   **Sizes**: 32×32, 128×128, 512×512 (dynamic); 3×3, 4×4, 8×8, 16×16 (fixed-size).

### Methodology

*   **Warmup**: 3 iterations before measurement.
*   **Measurement**: 20 iterations averaged (fewer for large matrix multiply).
*   **Timer**: `CLOCK_MONOTONIC` (C) / `std::chrono::high_resolution_clock` (C++).
*   **Reproducibility**: Same LCG seed used for both C and C++ random values.
*   **Optimization**: `-O2` for both C and C++.

---

## 3. Build Instructions

CMake 3.16+ and a C11/C++17 compiler are required.

```bash
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j
```

---

## 4. Running Benchmarks

### Unified Runner (Recommended)

Use the repository-level script to run **both generic and matrix** benchmarks and write tabular outputs suitable for plotting.

```bash
python3 scripts/run_all_benchmarks.py --build-dir build --output-dir benchmark_results --repeats 5
```

#### Advanced Testing Strategies

The runner supports flexible workload sizing via sequences or geometric progressions:

```bash
# Sequence of specific sizes
python3 scripts/run_all_benchmarks.py --build-dir build --sort "1000,10000,100000"

# Geometric progression (start, xMultiplier, steps)
python3 scripts/run_all_benchmarks.py --build-dir build --buffer "1048576,x2,4"
```

#### Representative Test Defaults

To get the most representative results (balancing cache effects and execution time), the following strategies are recommended:

| Group | Parameter      | Recommended Strategy | Reasoning                                               |
|:------|:---------------|:---------------------|:--------------------------------------------------------|
| **a** | `--sort`       | `1000000`            | Exceeds L3 cache (16MB), represents DRAM-bound sorting. |
| **b** | `--callback`   | `10000000`           | Measures micro-overhead of dispatch.                    |
| **c** | `--struct-api` | `10000000`           | Measures member access overhead.                        |
| **d** | `--buffer`     | `262144`             | Record count `N`; SoA hot set (~1MB) fits L3 while AoS (~16MB) is DRAM-bound. |
| **e** | `--table`      | `16777216`           | Dataset size in bytes; C applies the rounds at runtime, C++ fuses them at compile time into one lookup. |


Example command for representative run:

```bash
python3 scripts/run_all_benchmarks.py --skip-matrix --build-dir cmake-build --output-dir benchmark-results --repeats 5 --sort "10000,100000,1000000,10000000" --callback 10000000 --struct-api 10000000 --buffer "1048576,x2,4" --table 10000000
```

### Individual Execution

You can also run benchmarks directly:

```bash
# Generic
./build/generic_perf_compare/benchmarks/a_std_sort_cpp 1000000

# Matrix
./build/matrix_perf_compare/project/c_matrix_bench results/c_results.csv
```

---

## 5. Outputs

The unified runner produces the following artifacts in `--output-dir`:

- `runs.csv` - Long-format per-run timing table.
- `summary.csv` - Grouped summary statistics (mean, min, max, stdev).
- `runs.json` - Averaged results across repeats in JSON format.
- `results_{group}.csv` - Group-specific CSV files (e.g., `results_a.csv`).

### Report Compilation

After running benchmarks, you can generate a human-readable Markdown report:

```bash
python3 scripts/compile_report.py benchmark_results --output-format md --output-file report.md
```

#### Filtering Results

You can filter which results to include in the report using `--suites`, `--groups`, or `--benchmarks`:

```bash
# Only include group 'a' and 'b' from the generic suite
python3 scripts/compile_report.py benchmark_results --groups a,b
```

---

## 6. Interpretation Guidance

- **Ratio (C / C++) > 1.0x**: C is slower than C++.
- **Fixed-size vs Dynamic**: Eigen's fixed-size matrices (N ≤ 16) often show 2–10x gains due to loop unrolling and stack allocation.
- **Matrix Multiply**: Eigen's blocked algorithms typically outperform naive C loops by 5–20x for N=512.
- **Element-wise Ops**: Usually memory-bandwidth bound; expect ratios close to 1.0x for large matrices.

---

## License

See [LICENSE](LICENSE).
