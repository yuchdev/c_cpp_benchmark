# Benchmark Visualization

The visualization subsystem turns the benchmark results produced by the C/C++
benchmark framework into publication-quality Matplotlib charts that compare C
and C++ performance. It is implemented in [`scripts/plot_results.py`](../scripts/plot_results.py)
and wired into [`scripts/run_all_benchmarks.py`](../scripts/run_all_benchmarks.py). The matrix
*scenario* charts (heatmaps, sweeps, the convolution images) live in
[`scripts/plot_scenarios.py`](../scripts/plot_scenarios.py), which `plot_results.py` calls for you.

It is designed to visually demonstrate:

* performance differences,
* performance parity,
* scaling behavior, and
* speedup ratios

for inclusion in articles, reports, README files, and CI artifacts.

> Only **Matplotlib** and the Python standard library are used — no `seaborn`,
> `plotly`, `bokeh` or `pandas`. (NumPy is used by the heatmaps and the PGM reader, but it is
> always present: Matplotlib depends on it.) It works on Linux, macOS and Windows.

---

## Installation

```bash
python3 -m pip install matplotlib
```

---

## Input formats

The parser auto-detects two schemas.

### 1. Rich schema (`runs.csv` / `runs.json`)

This is the format emitted by `run_all_benchmarks.py`:

```csv
suite,group,benchmark,language,run,measure,metric,value,unit
generic,a,a_qsort_c,c,1,10000,wall_time_sec,0.000584,sec
generic,a,a_std_sort_cpp,cpp,1,10000,wall_time_sec,0.000273,sec
```

The equivalent JSON is a list of objects (or a `{"results": [...]}` wrapper):

```json
[
  {"suite": "generic", "group": "a", "benchmark": "a_qsort_c",
   "language": "c", "measure": 10000, "metric": "wall_time_sec",
   "value": 0.000564, "unit": "sec", "samples": 5}
]
```

### 2. Simple schema

A minimal CSV is also accepted:

```csv
implementation,operation,size,time_ns
c,sort,1000,1000
cpp,sort,1000,500
```

`time_ns` (nanoseconds) or `time_sec` (seconds) may be used for the value column.

All values are normalized to **seconds** internally so heterogeneous suites
(generic in `sec`, matrix in `ns`) can be compared.

---

## How operations are grouped

One chart is produced per **operation**. The grouping rule adapts to the data
layout inside each `(suite, group)`:

| Layout | Example | Operation key |
|:---|:---|:---|
| Distinct benchmark per language | generic group `a`: `a_qsort_c` (C) + `a_std_sort_cpp` (C++) | the group (`sort`) |
| Same benchmark name across languages | matrix `mul` exists for both C and C++ | the benchmark name (`mul`) |
| Matrix *scenario* (group `matrix_<scenario>`) | `matrix_fixed` / `mul` | `<group>/<benchmark>` → `matrix_fixed/mul` |

Scenario operations are namespaced so that, for example, the compile-time-N `mul` can never merge
into the core `mul`. They get no per-operation chart and no bar in `overall_speedup.png`; they are
drawn by the scenario figures below and listed in the `# Scenario Results` section of `summary.md`.

Friendly labels are applied to the generic single-letter groups:

| Group | Label |
|:---|:---|
| `a` | `sort` |
| `b` | `callback` |
| `c` | `struct_api` |
| `d` | `copy_move` |
| `e` | `lookup_table` |
| `f` | `fir` |

The `C` and `C++` series within each operation are taken from the `language`
column.

---

## Output

```text
benchmark-results/plots/
    sort.png
    callback.png
    matrix_multiply.png    # one PNG per operation
    ...
    overall_speedup.png    # cross-operation bar chart
    summary.md             # textual summary report (core + "# Scenario Results")

    speedup_heatmap.png    # core matrix op x size grid, coloured by speedup
    scenario_overview.png  # geometric-mean speedup of every scenario variant
    scenario_chain.png  chain_heatmap.png
    scenario_fixed.png  fixed_size_heatmap.png
    scenario_cliff.png  scenario_batch.png  scenario_block.png
    scenario_tri.png    scenario_conv.png   conv_images.png
```

### Per-operation chart

* **X axis** — benchmark scale. Numeric sizes use compact suffixes (`10K`,
  `1M`); matrix sizes render as `NxN` (`32x32`).
* **Y axis** — execution time, with the unit auto-selected from the largest
  measurement:

  | Largest measurement | Unit |
  |:---|:---|
  | `< 1000 ns` | `ns` |
  | `< 1 ms` | `µs` |
  | `< 1 s` | `ms` |
  | otherwise | `s` |

* **Series** — `C` and `C++` as separate lines (default) or grouped bars
  (`--chart bar`).
* **Legend** — embeds the average speedup, e.g. `C++ (2.14× faster)` or
  `C++ (1.87× slower)`, where `speedup = mean(c_time / cpp_time)` over all
  shared points.
* **Title / subtitle** — e.g. `Matrix Multiply Performance` with subtitle
  `Average speedup: 2.31×`.
* **Y scaling** — the axis maximum is `max(measurements) * 1.1` (10 % headroom).
* **Grid** — major grid only.
* **Data labels** — added only when an operation has `<= 10` points.

### `overall_speedup.png`

A bar chart with one bar per operation, where the height is the average
`C / C++` speedup. A dashed line at `1.0×` marks the parity threshold; bars at
or above parity are green, below are purple.

### `summary.md`

```md
# Benchmark Summary

## Sort

Average speedup:
3.10×

Best case:
4.36×

Worst case:
1.41×

Winner:
C++
```

---

## Scenario charts

The matrix scenarios are parameter sweeps (chain length, matrix size, block size, …), so they get
figures that show the whole sweep rather than one line per operation. Throughout, **green means
C++ is faster, purple means C is faster**, and speedup is always `C time / C++ time`; averages over a
sweep are *geometric* (a 2× win and a 2× loss cancel to 1×).

| File | What it shows |
|:---|:---|
| `speedup_heatmap.png` | Core grid: operation × matrix size, one cell per speedup. The colour scale is log₂, centred on 1×, so "2× faster" and "2× slower" are equally strong and opposite. Grey cells have no valid measurement. |
| `scenario_cliff.png` | Per-element time (ns) from L1- to DRAM-resident matrices, one column per operation; the bytes touched per call are printed under each size, so the cache steps line up with the speedup collapsing toward 1×. |
| `scenario_batch.png` | ns per point of one 4×4 transform over M points. |
| `scenario_block.png` | Block copy and block multiply inside 512×512 matrices; time per element / per multiply-add. |
| `scenario_tri.png` | Triangular solve and rank-k update; time per n³. |
| `scenario_conv.png` | ns per pixel of the 3×3 blur. |
| `scenario_chain.png` / `chain_heatmap.png` | Chain-length sweep: speedup per matrix size, absolute time at a cache-resident size, and the k × N heatmap. |
| `scenario_fixed.png` / `fixed_size_heatmap.png` | Compile-time-N sweep: speedup per operation versus N, absolute `mul` time, and the operation × N heatmap. |
| `conv_images.png` | The generated input image, the C blur, the C++ blur and their amplified difference, with the count of differing pixels. Needs the PGM files the benchmark writes to `<results>/matrix_raw/images/`; skipped when they are missing. |
| `scenario_overview.png` | One bar per scenario variant: geometric-mean speedup over its sweep. |

Each sweep figure has the normalised time (log y axis, both languages) on top and the speedup with a
1× reference line underneath. The figures that need data a run did not produce (for example a
`--scenarios chain` run has no cliff data) are skipped silently.

`plot_results.py <results>/runs.csv` finds the images next to the CSVs automatically
(`matrix_raw/images`).

---

## CLI usage

### `plot_results.py`

```bash
# Plot from a results file (CSV or JSON); plots go to benchmark-results/plots
python3 scripts/plot_results.py benchmark-results/runs.csv

# Choose an explicit output directory
python3 scripts/plot_results.py benchmark-results/runs.json --out results/plots

# Grouped-bar charts instead of lines
python3 scripts/plot_results.py benchmark-results/runs.csv --chart bar

# Article-optimized output (see below)
python3 scripts/plot_results.py benchmark-results/runs.csv --article-mode
```

| Option | Default | Description |
|:---|:---|:---|
| `input` (positional) | — | Results file (`.csv` or `.json`) |
| `--out DIR` | `benchmark-results/plots` | Output directory |
| `--chart {line,bar}` | `line` | Per-operation chart style |
| `--article-mode` | off | 840×420 landscape, larger fonts, thinner grid |

The command exits with code `2` on a missing file, unsupported extension,
unrecognized schema, or empty input.

### `run_all_benchmarks.py` integration

Plotting can run **during** a benchmark run or **after** an existing run:

```bash
# Run benchmarks and plot in one step
python3 scripts/run_all_benchmarks.py --build-dir build --output-dir benchmark-results --plot

# Only plot an already-completed results directory (no benchmarks executed)
python3 scripts/run_all_benchmarks.py --plot-only benchmark-results

# Article-mode bar charts to a custom directory
python3 scripts/run_all_benchmarks.py --plot-only benchmark-results \
    --plots-dir benchmark-results/plots --chart bar --article-mode
```

| Option | Description |
|:---|:---|
| `--plot` | Generate plots after the run completes |
| `--plot-only DIR` | Skip running; plot an existing results directory and exit |
| `--plots-dir DIR` | Plot output directory (default: `<output-dir>/plots`) |
| `--chart {line,bar}` | Per-operation chart style |
| `--article-mode` | Render 840×420 article-optimized plots |

`--plot-only` reads `runs.csv` if present, otherwise `runs.json`.

---

## Article mode

`--article-mode` produces **840×420** landscape PNGs with larger fonts and
thinner grid lines, optimized for embedding in Dev.to articles. The default
mode produces a slightly larger 4:2.5 chart suited to README files and reports.

---

## Tests

Unit tests live in [`tests/test_plot_results.py`](../tests/test_plot_results.py)
(CSV/JSON parsing, speedup calculation, unit selection, end-to-end plot generation),
[`tests/test_plot_scenarios.py`](../tests/test_plot_scenarios.py) (scenario namespacing, every
scenario figure, the PGM reader, heatmaps, partial-data robustness) and
[`tests/test_run_all_benchmarks.py`](../tests/test_run_all_benchmarks.py) (matrix row
classification and option forwarding, against fake benchmark binaries):

```bash
python3 -m unittest tests.test_plot_results tests.test_plot_scenarios tests.test_run_all_benchmarks -v
```
