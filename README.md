# C/C++ Benchmarks Collection

This repository contains a comprehensive set of C/C++ benchmarks split into two families:

1.  **Generic programming-pattern benchmarks** (`benchmarks/generic`)
2.  **Matrix-operation benchmarks** (`benchmarks/matrix`)

---

## Quick Start

Get results for all benchmarks with a single command (requires Python 3 and CMake):

```bash
python3 scripts/run_all_benchmarks.py --build-dir build --output-dir benchmark_results
```

The script will automatically configure (via CMake), build, and execute all benchmarks, saving results to the `benchmark_results` directory.

---

## 1. Generic Benchmarks (`benchmarks/generic`)

These are paired micro-benchmarks comparing equivalent C and C++ implementations of common programming patterns.

| Group | C source              | C++ source                  | Focus                                                       |
|:------|:----------------------|:----------------------------|:------------------------------------------------------------|
| **a** | `a_qsort_c.c`         | `a_std_sort_cpp.cpp`        | `qsort` callback dispatch vs `std::sort` + lambda           |
| **b** | `b_callback_c.c`      | `b_template_cpp.cpp`        | Function-pointer callbacks vs template/lambda transforms    |
| **c** | `c_struct_api.c`      | `c_class_operator.cpp`      | C struct-style API vs C++ class operators/methods           |
| **d** | `d_buffer_copy_c.c`   | `d_buffer_move_cpp.cpp`     | Cache locality: C Array-of-Structs vs C++ Structure-of-Arrays traversal |
| **e** | `e_runtime_table_c.c` | `e_constexpr_table_cpp.cpp` | Runtime multi-round S-box transform vs compile-time `constexpr` table fusion |

---

## 2. Matrix Benchmarks (`benchmarks/matrix`)

This suite compares handwritten C matrix routines against [Eigen](https://eigen.tuxfamily.org/) (a modern C++ linear algebra library) across a range of matrix sizes and operations.

### Scope

| Category           | What is benchmarked                                                                                         |
|:-------------------|:------------------------------------------------------------------------------------------------------------|
| **C (runtime)**    | Hand-written row-major `double` matrices; naive O(N³) multiply; `calloc`/`free` lifecycle                   |
| **C++ Dynamic**    | `Eigen::MatrixXd` - heap-allocated, column-major, SIMD-optimised, expression-template fusion                |
| **C++ Fixed-size** | `Eigen::Matrix<double, N, N>` - stack/register allocated, fully unrolled, maximum compile-time optimisation |

*   **Operations**: `transpose`, `add`, `sub`, `scale`, `matvec`, `mul`, `transpose_mul`, `add3`, `mul_add`.
*   **Sizes**: fully configurable (default 32×32, 128×128, 512×512 dynamic; 3×3, 4×4, 8×8, 16×16 fixed-size).

### Flexible CLI

Both `c_matrix_bench` and `cpp_matrix_bench` share the same options:
`--sizes` (list or `start:xMUL:steps` progression), `--ops`, `--warmup`,
`--iters`, `--repeats`, `--heavy-divisor`, `--seed`, `--csv`,
`--format table|csv|json`, `--no-fixed`, `--list-ops`, `--help`. Run
`./cpp_matrix_bench --help` for details. See
[docs/matrix_optimization.md](docs/matrix_optimization.md).

### Methodology

*   **Warmup**: `--warmup` iterations (default 3) before measurement.
*   **Measurement**: `--iters` iterations averaged (default 20; fewer for large multiply via `--heavy-divisor`), best of `--repeats`.
*   **Timer**: `CLOCK_MONOTONIC` (C) / `std::chrono::steady_clock` (C++).
*   **Reproducibility**: Same LCG seed used for both C and C++ random values.
*   **Optimization**: portable `-O2` for C vs aggressive `-O3 -march=native` (AVX2/FMA, Eigen SIMD) for C++; toggle with `-DMATRIX_CPP_AGGRESSIVE`.

---

## 3. Build Instructions

**Requirements:** CMake 3.16+, a C11 compiler, a C++17 compiler, Python 3.7+.
Eigen3 3.3+ is fetched automatically via `FetchContent` if not found on the system.
The C benchmarks use POSIX `clock_gettime(CLOCK_MONOTONIC, …)`, so a POSIX-compatible
toolchain is required on Windows (see below).

### Ubuntu / Debian

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake python3 python3-pip libeigen3-dev
pip3 install matplotlib

cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j$(nproc)
```

### macOS

Requires [Homebrew](https://brew.sh):

```bash
brew install cmake eigen python
pip3 install matplotlib

cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build -j$(sysctl -n hw.logicalcpu)
```

### Windows

#### Option A — MSYS2 / MinGW-w64 (recommended)

1. Install [MSYS2](https://www.msys2.org) and open the **UCRT64** shell.
2. Install the toolchain:

```bash
pacman -S --needed \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-eigen3 \
    python python-pip
pip install matplotlib
```

3. Build from the repository root inside the UCRT64 shell:

```bash
cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build
```

#### Option B — WSL2

Inside a WSL2 Ubuntu shell, follow the **Ubuntu / Debian** instructions above.

### CMake Options (all platforms)

```bash
# Build only one suite
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DBUILD_GENERIC_BENCHMARKS=OFF
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DBUILD_MATRIX_BENCHMARKS=OFF

# Disable aggressive C++ flags for a flag-equal comparison (both sides at -O2)
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release -DMATRIX_CPP_AGGRESSIVE=OFF
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

#### Everything, in One Command

`--all` runs both suites with the representative generic sizes above, sweeps every matrix
operation (`--matrix-ops all`) across a wide dynamic size range (`--matrix-sizes
4,8,16,32,64,128,256,512`, best-of-5 via `--matrix-repeats 5`; the fixed-size Eigen group always
covers 3×3/4×4/8×8/16×16 regardless of `--matrix-sizes`), and generates plots — no other flags
needed:

```bash
python3 scripts/run_all_benchmarks.py --build-dir cmake-build --output-dir benchmark-results --all
```

Any of `--sort`/`--callback`/`--struct-api`/`--buffer`/`--table`/`--matrix-sizes`/`--matrix-ops`/
`--matrix-repeats` passed alongside `--all` override just that one default; `--all` cannot be
combined with `--skip-generic`/`--skip-matrix`.

### Individual Execution

You can also run benchmarks directly:

```bash
# Generic
./cmake-build/benchmarks/generic/a_std_sort_cpp 1000000

# Matrix
./cmake-build/benchmarks/matrix/c_matrix_bench results/c_results.csv
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

### Visualization (Matplotlib)

Generate publication-quality charts (one PNG per operation, an overall speedup
bar chart, and a `summary.md`) from the results. Requires Matplotlib only
(`python3 -m pip install matplotlib`):

```bash
# Plot an existing results directory
python3 scripts/plot_results.py benchmark-results/runs.csv --out benchmark-results/plots

# Or run benchmarks and plot in one step
python3 scripts/run_all_benchmarks.py --build-dir build --output-dir benchmark-results --plot

# Plot an already-completed run without re-running benchmarks
python3 scripts/run_all_benchmarks.py --plot-only benchmark-results
```

Each chart shows C vs C++ as separate series with the average speedup embedded
in the legend. Use `--chart bar` for grouped bars and `--article-mode` for
840×420 images tuned for articles. See
[docs/benchmark_visualization.md](docs/benchmark_visualization.md) for full
details.

---

## 6. Interpretation Guidance

- **Ratio (C / C++) > 1.0x**: C is slower than C++.
- **Fixed-size vs Dynamic**: Eigen's fixed-size matrices (N ≤ 16) often show 2–10x gains due to loop unrolling and stack allocation.
- **Matrix Multiply**: Eigen's blocked algorithms typically outperform naive C loops by 5–20x for N=512.
- **Element-wise Ops**: Usually memory-bandwidth bound; expect ratios close to 1.0x for large matrices.

---

## License

See [LICENSE](LICENSE).
