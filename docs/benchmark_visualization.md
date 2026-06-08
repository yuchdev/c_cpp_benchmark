# Benchmark Visualization

The visualization subsystem turns the benchmark results produced by the C/C++
benchmark framework into publication-quality Matplotlib charts that compare C
and C++ performance. It is implemented in [`scripts/plot_results.py`](../scripts/plot_results.py)
and wired into [`scripts/run_all_benchmarks.py`](../scripts/run_all_benchmarks.py).

It is designed to visually demonstrate:

* performance differences,
* performance parity,
* scaling behavior, and
* speedup ratios

for inclusion in articles, reports, README files, and CI artifacts.

> Only **Matplotlib** and the Python standard library are used — no `seaborn`,
> `plotly`, `bokeh`, `pandas` or `numpy` are required. It works on Linux, macOS
> and Windows.

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

Friendly labels are applied to the generic single-letter groups:

| Group | Label |
|:---|:---|
| `a` | `sort` |
| `b` | `callback` |
| `c` | `struct_api` |
| `d` | `copy_move` |
| `e` | `lookup_table` |

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
    summary.md             # textual summary report
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
and cover CSV/JSON parsing, speedup calculation, unit selection and end-to-end
plot generation:

```bash
python3 -m unittest tests.test_plot_results -v
```
