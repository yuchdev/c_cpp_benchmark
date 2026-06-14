# Copilot Instructions — `c_cpp_benchmark`

This repository is a reproducible C vs C++ micro-benchmark collection. When assisting with code
in this project, follow the guidelines below.

---

## Project Overview

Two benchmark suites live side by side:

- **`benchmarks/generic/`** — Five paired benchmarks (groups a–e) each isolating one compiler
  lever: function inlining (A), template dispatch (B), inline class operators (C), data layout /
  cache (D), and `constexpr` compile-time evaluation (E). Each group has one `.c` file and one
  `.cpp` file that perform identical logical work.
- **`benchmarks/matrix/`** — Matrix operations (transpose, add, mul, matvec, …) comparing
  hand-written portable C against Eigen (C++) at configurable sizes. Includes CTest unit tests.

Supporting scripts (`scripts/`) orchestrate builds, run benchmarks, produce CSV/JSON results, and
generate Matplotlib charts. Python tests live in `tests/`.

---

## Language & Standards

- **C sources** (`*.c`): C11. `#include` only standard headers and project-local headers.
- **C++ sources** (`*.cpp`, `*.hpp`): C++17. Use lambdas, templates, `constexpr`, and
  `std::chrono::steady_clock`.
- **Python scripts**: Python 3.7+. Only standard library plus Matplotlib (optional, for charts).

---

## Code Style

All C/C++ code is formatted with **clang-format** (see `.clang-format`):

- Indent: **4 spaces** (no tabs)
- Brace style: **Allman** (opening brace on its own line)
- Pointer alignment: **left** (`int* p`)
- No column limit

CMake files use **cmake-format** (see `.cmake-format`):

- Indent: **4 spaces**, line width: **100 characters**

When generating or modifying C/C++ code, always match the clang-format style. When modifying
`CMakeLists.txt`, match the cmake-format style.

---

## Build System

- Build tool: **CMake 3.16+**
- Build type for benchmarks: always **Release**
- C baseline flags: `-O2` (portable, no machine-specific tuning)
- C++ aggressive flags: `-O3 -march=native -funroll-loops -fno-math-errno -ffp-contract=fast`
  (GCC/Clang only, controlled by `MATRIX_CPP_AGGRESSIVE=ON`)
- Eigen3 is fetched automatically via `FetchContent` if not found via `find_package`
- **Windows**: the C benchmarks use `clock_gettime(CLOCK_MONOTONIC, …)`; build under
  MSYS2/MinGW-w64 or WSL2 — pure MSVC is not supported

**Ubuntu / Debian:**
```bash
sudo apt-get install -y build-essential cmake python3 python3-pip libeigen3-dev
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release && cmake --build cmake-build -j$(nproc)
```

**macOS:**
```bash
brew install cmake eigen python
cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release && cmake --build cmake-build -j$(sysctl -n hw.logicalcpu)
```

**Windows (MSYS2 UCRT64 shell):**
```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-eigen3 python python-pip
cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build cmake-build
```

---

## Testing

- **C/C++ tests** (correctness, CLI options, Eigen): `ctest --test-dir cmake-build --output-on-failure`
- **Python tests**: `python3 -m unittest tests.test_plot_results -v`

Always run both test suites after changes. Do not add new test frameworks unless asked.

---

## Benchmark Invariants

These rules apply to all benchmark code — never violate them:

1. **C and C++ must perform identical logical work** — same data, same algorithm, same result.
2. **Anti-optimization sinks**: after the timed loop, read an output element via a `volatile`
   access to prevent dead-code elimination.
3. **No I/O inside the timed region** — `printf` and CSV writes happen before/after measurement.
4. **LCG seed constants are fixed**: multiplier `1664525`, addend `1013904223`. Use the same seed
   in both C and C++ drivers.
5. **Warmup** before measuring; keep the **minimum average** of `--repeats` runs.
6. **Single-threaded only** — do not enable Eigen OpenMP or C++ thread pools.

---

## Key Patterns

### C — intentional `volatile` callback (do not "fix")

```c
// The volatile global is intentional — prevents devirtualization of the callback
static volatile op_t g_op = my_transform;
```

### C++ — lambda template (prefer over function pointer)

```cpp
template <typename Op>
void transform_buffer(int* data, size_t n, Op op)
{
    for (size_t i = 0; i < n; ++i)
        data[i] = op(data[i]);
}
```

### C++ — constexpr table (prefer for compile-time data)

```cpp
constexpr auto fused = make_fused_table(); // evaluated at compile time
```

### Eigen — always use `.noalias()` for GEMM

```cpp
C.noalias() = A * B;       // correct: no defensive temporary
C = A * B;                 // avoid: Eigen allocates a temporary
```

---

## What to Avoid

- **`virtual` functions** in benchmark hot paths — defeats the purpose of the benchmarks.
- **Heap allocation** inside the timed region for matrix benchmarks.
- **Changing LCG constants** without updating both C and C++ drivers.
- **`Debug` builds** for benchmark runs — results are meaningless without optimization.
- **External BLAS** (OpenBLAS, MKL) linked into Eigen benchmark targets.
- **Non-portable compiler flags** added globally via `add_compile_options`.

---

## Repository Documentation

| Document | What it covers |
|---|---|
| `README.md` | Quick start, suite overview, build, CLI reference |
| `AGENTS.md` | Full agentic guidelines (this project's source of truth for agents) |
| `docs/c_cpp_benchmarking.md` | Narrative walkthrough of all five generic tests |
| `docs/generic_benchmark_methodology.md` | Warmup/measurement/fairness rules |
| `docs/matrix_benchmark_methodology.md` | Matrix suite architecture, optimization strategy, results |
| `docs/benchmark_visualization.md` | `plot_results.py` full CLI and output reference |
| `docs/compiler_explorer.md` | Godbolt assembly examples for each benchmark |
