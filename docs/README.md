# Documentation Index

This directory contains reference and methodology documentation for the
`c_cpp_benchmark` project. The table below lists every document with a short
description; the [recommended reading order](#recommended-reading-order) follows.

---

## Documents

| File | Title | What it covers |
|:-----|:------|:---------------|
| [`c_cpp_benchmarking.md`](c_cpp_benchmarking.md) | When C++ Is Faster Than C | Narrative walkthrough of all five generic benchmarks (A–E) with measured results and detailed explanations of the compiler levers each test isolates |
| [`generic_benchmark_methodology.md`](generic_benchmark_methodology.md) | Benchmark Methodology | Measurement design: warmup strategy, timing approach, fairness rules (same element type, same seed, no I/O in timed region), and result-interpretation guidance |
| [`matrix_benchmark_methodology.md`](matrix_benchmark_methodology.md) | Matrix Benchmarking | Architecture of the `benchmarks/matrix` suite, CLI reference, C vs C++ optimization strategy, and concrete measured results for compute-bound and element-wise operations |
| [`benchmark_visualization.md`](benchmark_visualization.md) | Benchmark Visualization | Full reference for `scripts/plot_results.py`: input formats, chart types, output layout, CLI options, and integration with `run_all_benchmarks.py` |
| [`compiler_explorer.md`](compiler_explorer.md) | Compiler Explorer Examples | Annotated Godbolt snippets for each benchmark; shows the assembly-level difference between C and C++ implementations side by side |

---

## Recommended Reading Order

### 1. [`c_cpp_benchmarking.md`](c_cpp_benchmarking.md) — Start here

Explains *why* C++ is faster in each of the five generic tests. Covers inlining,
template dispatch, inline class operators, cache-optimal data layout, and
`constexpr` compile-time evaluation. Includes measured timings and rationale for
why C cannot match the C++ idiom without sacrificing generality or type safety.

> **Prerequisite:** none. Good starting point for anyone unfamiliar with the project.

---

### 2. [`generic_benchmark_methodology.md`](generic_benchmark_methodology.md) — How results are produced

Describes the measurement harness: warmup iterations, timing with
`CLOCK_MONOTONIC` / `std::chrono::steady_clock`, best-of-repeats strategy, and
the fairness rules that make C/C++ comparisons meaningful (identical element type,
identical LCG seed, no I/O inside timed regions, `volatile` anti-optimization
sinks). Also explains how to interpret ratios and known limitations.

> **Prerequisite:** read (1) first to understand what is being measured.

---

### 3. [`matrix_benchmark_methodology.md`](matrix_benchmark_methodology.md) — The matrix suite in depth

Covers the `benchmarks/matrix` suite architecture, the shared CLI
(`--sizes`, `--ops`, `--repeats`, `--format`, …), the intentional optimization
asymmetry (C at `-O2` vs C++ at `-O3 -march=native` with Eigen SIMD), and
a table of concrete results for compute-bound operations (5–10× ratios) and
element-wise operations (~1× memory-bandwidth-bound).

> **Prerequisite:** read (1) and (2) for context on methodology and goals.

---

### 4. [`benchmark_visualization.md`](benchmark_visualization.md) — Turning results into charts

Reference for `scripts/plot_results.py`. Explains input formats (`runs.csv`,
`runs.json`, simple schema), how operations are grouped, what each output file
contains (`<op>.png`, `overall_speedup.png`, `summary.md`), and all CLI flags
including `--chart bar` and `--article-mode`.

> **Prerequisite:** read (3) so you understand the result format the charts are built from.

---

### 5. [`compiler_explorer.md`](compiler_explorer.md) — Assembly deep-dive (advanced)

Annotated examples for Godbolt (godbolt.org): fixed-size 4×4 transpose, 4×4
multiply, expression-template fusion, and matvec. Shows how to compare C and C++
assembly side by side and which compiler flags to use. Useful for understanding
*how* the measured performance differences manifest in generated code.

> **Prerequisite:** optional. Read any of (1)–(4) first; useful when investigating
> a specific benchmark result.
