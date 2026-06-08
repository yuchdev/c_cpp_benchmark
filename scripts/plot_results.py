#!/usr/bin/env python3
"""Benchmark visualization with Matplotlib.

Consumes benchmark results produced by the C/C++ benchmark framework (CSV or
JSON) and produces publication-quality plots that compare C and C++ performance:

* one line chart per operation (e.g. ``sort.png``, ``mul.png``),
* an ``overall_speedup.png`` bar chart, and
* a ``summary.md`` report.

Only Matplotlib and the Python standard library are used.

Supported input schemas
------------------------
1. Rich schema (``runs.csv`` / ``runs.json``)::

       suite,group,benchmark,language,run,measure,metric,value,unit

2. Simple schema (as referenced by the unit tests)::

       implementation,operation,size,time_ns

Usage
-----
    python plot_results.py benchmark.csv
    python plot_results.py benchmark.json --out results/plots
    python plot_results.py benchmark.csv --article-mode
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# Friendly labels for the generic-suite single-letter groups.
GROUP_LABELS = {
    "a": "sort",
    "b": "callback",
    "c": "struct_api",
    "d": "copy_move",
    "e": "lookup_table",
}

# Canonical language identifiers and how they appear in legends/titles.
LANG_C = "c"
LANG_CPP = "cpp"
LANG_DISPLAY = {LANG_C: "C", LANG_CPP: "C++"}


@dataclass(frozen=True)
class BenchmarkResult:
    """A single aggregated benchmark measurement.

    ``time_sec`` is always normalized to seconds regardless of the unit used in
    the source file, so that downstream code can compare heterogeneous suites.
    """

    suite: str
    operation: str
    language: str
    size: float
    time_sec: float
    unit: str = "sec"
    samples: int = 1


# ---------------------------------------------------------------------------
# Unit handling
# ---------------------------------------------------------------------------

#: Multipliers that convert a value expressed in ``unit`` into seconds.
_UNIT_TO_SEC = {
    "ns": 1e-9,
    "nanoseconds": 1e-9,
    "us": 1e-6,
    "µs": 1e-6,
    "μs": 1e-6,
    "microseconds": 1e-6,
    "ms": 1e-3,
    "milliseconds": 1e-3,
    "s": 1.0,
    "sec": 1.0,
    "secs": 1.0,
    "seconds": 1.0,
    "wall_time_sec": 1.0,
}


def to_seconds(value: float, unit: str) -> float:
    """Convert ``value`` expressed in ``unit`` to seconds.

    :param value: Numeric measurement.
    :param unit: Unit string (case-insensitive); unknown units are treated as
        seconds.
    :returns: The measurement expressed in seconds.
    """
    factor = _UNIT_TO_SEC.get(str(unit).strip().lower(), 1.0)
    return value * factor


def select_unit(max_time_sec: float) -> Tuple[str, float]:
    """Pick a human-friendly display unit for a maximum measurement.

    The rule mirrors the specification: the unit is chosen so that the largest
    measurement renders with a compact magnitude.

    :param max_time_sec: Largest measurement in the chart, in seconds.
    :returns: A ``(label, scale)`` pair where ``value_sec / scale`` yields the
        value in the chosen unit.
    """
    if max_time_sec <= 0 or math.isnan(max_time_sec):
        return "ns", 1e-9
    if max_time_sec < 1e-6:        # < 1000 ns
        return "ns", 1e-9
    if max_time_sec < 1e-3:        # < 1 ms
        return "µs", 1e-6
    if max_time_sec < 1.0:         # < 1 s
        return "ms", 1e-3
    return "s", 1.0


# ---------------------------------------------------------------------------
# Operation / label helpers
# ---------------------------------------------------------------------------

def normalize_language(raw: str) -> str:
    """Map a raw language/implementation token to ``c`` or ``cpp``.

    :param raw: Token such as ``"c"``, ``"cpp"``, ``"C++"`` or ``"cxx"``.
    :returns: ``"cpp"`` for any C++ spelling, otherwise ``"c"``.
    """
    token = str(raw).strip().lower()
    if token in ("cpp", "c++", "cxx", "cplusplus"):
        return LANG_CPP
    return LANG_C


def humanize_size(size: float, suite: str) -> str:
    """Format an X-axis size label.

    Matrix sizes are rendered as ``NxN``; other sizes use compact SI-style
    suffixes (``10K``, ``1M``, ...).

    :param size: Numeric size/measure.
    :param suite: Owning suite name (``"matrix"`` triggers ``NxN`` formatting).
    :returns: A short human-readable label.
    """
    n = int(round(size))
    if suite == "matrix":
        return f"{n}x{n}"
    if n >= 1_000_000_000 and n % 1_000_000_000 == 0:
        return f"{n // 1_000_000_000}G"
    if n >= 1_000_000 and n % 1_000_000 == 0:
        return f"{n // 1_000_000}M"
    if n >= 1_000 and n % 1_000 == 0:
        return f"{n // 1_000}K"
    return str(n)


def title_for(operation: str) -> str:
    """Build a chart title from an operation key.

    :param operation: Operation identifier such as ``"matrix_multiply"``.
    :returns: A title like ``"Matrix Multiply Performance"``.
    """
    words = re.split(r"[_\s]+", operation.strip())
    pretty = " ".join(w.capitalize() for w in words if w)
    return f"{pretty} Performance"


def format_speedup(ratio: float) -> str:
    """Render a speedup ratio as e.g. ``"2.31×"``.

    :param ratio: Speedup ratio.
    :returns: The ratio rounded to two decimals with a ``×`` suffix.
    """
    return f"{ratio:.2f}×"


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

def _operation_for_rich(suite: str, group: str, benchmark: str,
                        split_groups: set) -> str:
    """Resolve the operation key for a rich-schema record.

    Two layouts are supported within a ``(suite, group)``:

    * *Distinct-per-language* (e.g. the generic suite, where group ``a`` holds
      ``a_qsort_c`` for C and ``a_std_sort_cpp`` for C++). Here the whole group
      represents a single operation, labelled by the group.
    * *Shared-name* (e.g. the matrix suite, where ``mul`` exists for both C and
      C++). Here each benchmark name is its own operation.

    :param suite: Suite name.
    :param group: Group name.
    :param benchmark: Benchmark name.
    :param split_groups: Set of ``(suite, group)`` keys that use the
        shared-name layout and must be split per benchmark.
    :returns: The operation key/label.
    """
    if (suite, group) in split_groups:
        return benchmark
    return GROUP_LABELS.get(group, group)


def _rows_to_results(raw_rows: List[Dict[str, object]]) -> List[BenchmarkResult]:
    """Aggregate raw rich-schema rows into :class:`BenchmarkResult` records.

    Rows sharing ``(suite, operation, language, size)`` are averaged so that
    multiple ``run`` samples collapse into a single point.

    :param raw_rows: Normalized dictionaries with rich-schema keys.
    :returns: Aggregated results, one per ``(suite, operation, language, size)``.
    """
    # Decide layout per (suite, group): if any benchmark name occurs under more
    # than one language, the group uses the shared-name layout (split per
    # benchmark, e.g. the matrix suite); otherwise it is a single operation.
    bench_langs: Dict[Tuple[str, str, str], set] = {}
    for r in raw_rows:
        key = (str(r["suite"]), str(r["group"]), str(r["benchmark"]))
        bench_langs.setdefault(key, set()).add(normalize_language(r["language"]))
    split_groups = {
        (suite, group)
        for (suite, group, _bench), langs in bench_langs.items()
        if len(langs) > 1
    }

    buckets: Dict[Tuple[str, str, str, float], List[Tuple[float, str]]] = {}
    for r in raw_rows:
        suite = str(r["suite"])
        operation = _operation_for_rich(
            suite, str(r["group"]), str(r["benchmark"]), split_groups
        )
        lang = normalize_language(r["language"])
        size = float(r["measure"])
        unit = str(r.get("unit", "sec"))
        sec = to_seconds(float(r["value"]), unit)
        buckets.setdefault((suite, operation, lang, size), []).append((sec, unit))

    results: List[BenchmarkResult] = []
    for (suite, operation, lang, size), entries in buckets.items():
        times = [e[0] for e in entries]
        results.append(
            BenchmarkResult(
                suite=suite,
                operation=operation,
                language=lang,
                size=size,
                time_sec=sum(times) / len(times),
                unit=entries[0][1],
                samples=len(times),
            )
        )
    return results


def parse_records(raw_rows: List[Dict[str, object]]) -> List[BenchmarkResult]:
    """Convert a list of dictionaries (rich or simple schema) to results.

    The schema is auto-detected from the available keys.

    :param raw_rows: Decoded rows from CSV or JSON.
    :returns: Aggregated benchmark results.
    :raises ValueError: If the schema cannot be recognized.
    """
    if not raw_rows:
        return []

    keys = set(raw_rows[0].keys())

    # Rich schema (runs.csv / runs.json).
    if {"benchmark", "language", "measure", "value"}.issubset(keys):
        normalized = []
        for r in raw_rows:
            normalized.append(
                {
                    "suite": r.get("suite", "default"),
                    "group": r.get("group", r.get("benchmark")),
                    "benchmark": r.get("benchmark"),
                    "language": r.get("language"),
                    "measure": r.get("measure"),
                    "value": r.get("value"),
                    "unit": r.get("unit", "sec"),
                }
            )
        return _rows_to_results(normalized)

    # Simple schema: implementation,operation,size,time_ns
    if {"implementation", "operation", "size"}.issubset(keys):
        time_key = "time_ns" if "time_ns" in keys else (
            "time_sec" if "time_sec" in keys else None
        )
        if time_key is None:
            raise ValueError("simple schema requires a 'time_ns' or 'time_sec' column")
        unit = "ns" if time_key == "time_ns" else "sec"
        normalized = []
        for r in raw_rows:
            normalized.append(
                {
                    "suite": r.get("suite", "default"),
                    "group": r.get("operation"),
                    "benchmark": r.get("operation"),
                    "language": r.get("implementation"),
                    "measure": r.get("size"),
                    "value": r.get(time_key),
                    "unit": unit,
                }
            )
        # In the simple schema each operation is its own group, so force the
        # per-benchmark split by giving every group a high benchmark count.
        return _rows_to_results_simple(normalized)

    raise ValueError(f"Unrecognized benchmark schema with columns: {sorted(keys)}")


def _rows_to_results_simple(raw_rows: List[Dict[str, object]]) -> List[BenchmarkResult]:
    """Aggregate simple-schema rows; the operation is taken verbatim.

    :param raw_rows: Normalized dictionaries (operation already in ``group``).
    :returns: Aggregated results.
    """
    buckets: Dict[Tuple[str, str, str, float], List[Tuple[float, str]]] = {}
    for r in raw_rows:
        suite = str(r["suite"])
        operation = str(r["group"])
        lang = normalize_language(r["language"])
        size = float(r["measure"])
        unit = str(r.get("unit", "sec"))
        sec = to_seconds(float(r["value"]), unit)
        buckets.setdefault((suite, operation, lang, size), []).append((sec, unit))

    results: List[BenchmarkResult] = []
    for (suite, operation, lang, size), entries in buckets.items():
        times = [e[0] for e in entries]
        results.append(
            BenchmarkResult(
                suite=suite,
                operation=operation,
                language=lang,
                size=size,
                time_sec=sum(times) / len(times),
                unit=entries[0][1],
                samples=len(times),
            )
        )
    return results


def load_results(path: Path) -> List[BenchmarkResult]:
    """Load and parse benchmark results from a CSV or JSON file.

    :param path: Path to a ``.csv`` or ``.json`` file.
    :returns: Aggregated benchmark results.
    :raises FileNotFoundError: If ``path`` does not exist.
    :raises ValueError: If the file extension or schema is unsupported.
    """
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"Input file not found: {path}")

    suffix = path.suffix.lower()
    if suffix == ".json":
        with path.open("r", encoding="utf-8") as f:
            data = json.load(f)
        if isinstance(data, dict):
            data = data.get("results", [])
        raw_rows = list(data)
    elif suffix == ".csv":
        with path.open(newline="", encoding="utf-8") as f:
            raw_rows = list(csv.DictReader(f))
    else:
        raise ValueError(f"Unsupported input extension '{suffix}' (use .csv or .json)")

    return parse_records(raw_rows)


# ---------------------------------------------------------------------------
# Analysis
# ---------------------------------------------------------------------------

@dataclass
class OperationData:
    """All series and derived statistics for a single operation/plot."""

    operation: str
    suite: str
    #: Mapping ``language -> {size: time_sec}``.
    series: Dict[str, Dict[float, float]]

    @property
    def sizes(self) -> List[float]:
        """Sorted union of all sizes present across languages."""
        seen = set()
        for points in self.series.values():
            seen.update(points.keys())
        return sorted(seen)

    @property
    def max_time_sec(self) -> float:
        """Largest measurement across every series (used for unit scaling)."""
        values = [t for points in self.series.values() for t in points.values()]
        return max(values) if values else 0.0

    def speedups(self) -> List[float]:
        """Per-size ``c_time / cpp_time`` ratios where both languages exist.

        :returns: One ratio per common size (empty if the pair is incomplete).
        """
        c = self.series.get(LANG_C, {})
        cpp = self.series.get(LANG_CPP, {})
        out = []
        for size in sorted(set(c) & set(cpp)):
            # Skip non-positive measurements (e.g. corrupt zero rows) that would
            # otherwise yield meaningless 0.00x or infinite ratios.
            if c[size] > 0 and cpp[size] > 0:
                out.append(c[size] / cpp[size])
        return out

    def average_speedup(self) -> Optional[float]:
        """Mean of :meth:`speedups`, or ``None`` when not computable."""
        ratios = self.speedups()
        return sum(ratios) / len(ratios) if ratios else None


def group_operations(results: List[BenchmarkResult]) -> Dict[str, OperationData]:
    """Bucket flat results into per-operation series.

    :param results: Aggregated benchmark results.
    :returns: Ordered mapping of operation key to :class:`OperationData`.
    """
    ops: Dict[str, OperationData] = {}
    for r in results:
        od = ops.get(r.operation)
        if od is None:
            od = OperationData(operation=r.operation, suite=r.suite, series={})
            ops[r.operation] = od
        od.series.setdefault(r.language, {})[r.size] = r.time_sec
    return ops


def winner_for(avg_speedup: Optional[float]) -> str:
    """Return the winning language label for an average speedup.

    :param avg_speedup: ``c_time / cpp_time`` average, or ``None``.
    :returns: ``"C++"``, ``"C"`` or ``"tie"``.
    """
    if avg_speedup is None:
        return "n/a"
    if avg_speedup > 1.0:
        return "C++"
    if avg_speedup < 1.0:
        return "C"
    return "tie"


def legend_label(language: str, avg_speedup: Optional[float]) -> str:
    """Compose a legend entry that embeds speedup context.

    The C++ entry is annotated relative to C: ``"C++ (2.14× faster)"`` when C++
    wins, ``"C++ (1.87× slower)"`` otherwise.

    :param language: ``"c"`` or ``"cpp"``.
    :param avg_speedup: Average ``c_time / cpp_time`` ratio.
    :returns: A legend label string.
    """
    base = LANG_DISPLAY.get(language, language)
    if language != LANG_CPP or avg_speedup is None or avg_speedup == 1.0:
        return base
    if avg_speedup > 1.0:
        return f"{base} ({format_speedup(avg_speedup)} faster)"
    return f"{base} ({format_speedup(1.0 / avg_speedup)} slower)"


# ---------------------------------------------------------------------------
# Plotting
# ---------------------------------------------------------------------------

def _sanitize_filename(name: str) -> str:
    """Turn an operation key into a safe PNG file stem.

    :param name: Operation key.
    :returns: A lower-case, filesystem-safe stem.
    """
    stem = re.sub(r"[^A-Za-z0-9._-]+", "_", name.strip().lower())
    return stem.strip("_") or "operation"


#: Per-language plot colours and markers (kept stable across charts).
_STYLE = {
    LANG_C: {"color": "#1f77b4", "marker": "o"},
    LANG_CPP: {"color": "#d62728", "marker": "s"},
}


def _apply_style(article_mode: bool) -> Dict[str, object]:
    """Return a bundle of style parameters for the requested mode.

    :param article_mode: When true, optimize for Dev.to landscape articles
        (840x420 px, larger fonts, thinner grid).
    :returns: Dictionary with ``figsize``, ``dpi``, ``title_size``,
        ``label_size``, ``tick_size``, ``legend_size``, ``grid_lw`` and
        ``line_lw`` keys.
    """
    if article_mode:
        dpi = 100
        return {
            "figsize": (840 / dpi, 420 / dpi),
            "dpi": dpi,
            "title_size": 18,
            "label_size": 14,
            "tick_size": 12,
            "legend_size": 13,
            "grid_lw": 0.4,
            "line_lw": 2.4,
        }
    dpi = 110
    return {
        "figsize": (8.0, 5.0),
        "dpi": dpi,
        "title_size": 14,
        "label_size": 11,
        "tick_size": 10,
        "legend_size": 10,
        "grid_lw": 0.8,
        "line_lw": 1.8,
    }


def plot_operation(op: OperationData, out_dir: Path, *, chart: str = "line",
                   article_mode: bool = False) -> Path:
    """Render and save the chart for a single operation.

    :param op: Operation data bundle.
    :param out_dir: Directory to write the PNG into.
    :param chart: ``"line"`` (default) or ``"bar"``.
    :param article_mode: Enable Dev.to-optimized styling.
    :returns: Path to the written PNG file.
    """
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    style = _apply_style(article_mode)
    sizes = op.sizes
    unit_label, scale = select_unit(op.max_time_sec)
    avg_speedup = op.average_speedup()

    fig, ax = plt.subplots(figsize=style["figsize"], dpi=style["dpi"])

    x = list(range(len(sizes)))
    width = 0.38  # bar width per language
    languages = [lang for lang in (LANG_C, LANG_CPP) if lang in op.series]

    for idx, lang in enumerate(languages):
        points = op.series[lang]
        y = [points.get(s, float("nan")) / scale for s in sizes]
        label = legend_label(lang, avg_speedup)
        st = _STYLE.get(lang, {"color": None, "marker": "o"})
        if chart == "bar":
            offset = (idx - (len(languages) - 1) / 2.0) * width
            bars = ax.bar([xi + offset for xi in x], y, width=width,
                          label=label, color=st["color"])
            if len(sizes) <= 10:
                ax.bar_label(bars, fmt="%.2f", fontsize=style["tick_size"] - 2,
                             padding=2)
        else:
            ax.plot(x, y, marker=st["marker"], color=st["color"],
                    linewidth=style["line_lw"], markersize=6, label=label)
            if len(sizes) <= 10:
                for xi, yi in zip(x, y):
                    if not math.isnan(yi):
                        ax.annotate(f"{yi:.2f}", (xi, yi),
                                    textcoords="offset points", xytext=(0, 6),
                                    ha="center", fontsize=style["tick_size"] - 2)

    # Axes, title, grid.
    ax.set_xticks(x)
    ax.set_xticklabels([humanize_size(s, op.suite) for s in sizes],
                       fontsize=style["tick_size"])
    ax.tick_params(axis="y", labelsize=style["tick_size"])
    ax.set_xlabel("Benchmark size", fontsize=style["label_size"])
    ax.set_ylabel(f"Execution time ({unit_label})", fontsize=style["label_size"])

    title = title_for(op.operation)
    subtitle = ("Average speedup: " + format_speedup(avg_speedup)
                if avg_speedup is not None else "Single-language data")
    ax.set_title(f"{title}\n{subtitle}", fontsize=style["title_size"])

    # Y headroom: 10% above the largest measurement.
    max_y = (op.max_time_sec / scale) * 1.1
    if max_y > 0:
        ax.set_ylim(0, max_y)

    ax.grid(True, which="major", linewidth=style["grid_lw"], alpha=0.6)
    ax.legend(fontsize=style["legend_size"])
    fig.tight_layout()

    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / f"{_sanitize_filename(op.operation)}.png"
    fig.savefig(out_path)
    plt.close(fig)
    return out_path


def plot_overall_speedup(ops: Dict[str, OperationData], out_dir: Path, *,
                         article_mode: bool = False) -> Optional[Path]:
    """Render the ``overall_speedup.png`` bar chart across all operations.

    :param ops: Mapping of operation key to data.
    :param out_dir: Output directory.
    :param article_mode: Enable Dev.to-optimized styling.
    :returns: Path to the PNG, or ``None`` if no operation has a speedup.
    """
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    pairs = [(name, od.average_speedup()) for name, od in ops.items()]
    pairs = [(n, s) for n, s in pairs if s is not None]
    if not pairs:
        return None

    style = _apply_style(article_mode)
    names = [p[0] for p in pairs]
    speedups = [p[1] for p in pairs]
    colors = ["#2ca02c" if s >= 1.0 else "#9467bd" for s in speedups]

    fig, ax = plt.subplots(figsize=style["figsize"], dpi=style["dpi"])
    x = list(range(len(names)))
    bars = ax.bar(x, speedups, color=colors)
    if len(names) <= 10:
        ax.bar_label(bars, fmt="%.2f×", fontsize=style["tick_size"] - 1,
                     padding=2)

    ax.axhline(1.0, color="gray", linestyle="--", linewidth=style["grid_lw"])
    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=30, ha="right", fontsize=style["tick_size"])
    ax.tick_params(axis="y", labelsize=style["tick_size"])
    ax.set_ylabel("Average speedup (C / C++)", fontsize=style["label_size"])
    ax.set_title("Overall C++ Speedup by Operation", fontsize=style["title_size"])

    max_y = max(speedups) * 1.1
    ax.set_ylim(0, max_y if max_y > 0 else 1.0)
    ax.grid(True, which="major", axis="y", linewidth=style["grid_lw"], alpha=0.6)
    fig.tight_layout()

    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "overall_speedup.png"
    fig.savefig(out_path)
    plt.close(fig)
    return out_path


# ---------------------------------------------------------------------------
# Summary report
# ---------------------------------------------------------------------------

def build_summary(ops: Dict[str, OperationData]) -> str:
    """Build the Markdown ``summary.md`` content.

    :param ops: Mapping of operation key to data.
    :returns: Markdown text.
    """
    lines = ["# Benchmark Summary", ""]
    for name, od in ops.items():
        ratios = od.speedups()
        title = title_for(od.operation).replace(" Performance", "")
        lines.append(f"## {title}")
        lines.append("")
        if not ratios:
            lines.append("Single-language data; no speedup available.")
            lines.append("")
            continue
        avg = sum(ratios) / len(ratios)
        lines.append("Average speedup:")
        lines.append(format_speedup(avg))
        lines.append("")
        lines.append("Best case:")
        lines.append(format_speedup(max(ratios)))
        lines.append("")
        lines.append("Worst case:")
        lines.append(format_speedup(min(ratios)))
        lines.append("")
        lines.append("Winner:")
        lines.append(winner_for(avg))
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def write_summary(ops: Dict[str, OperationData], out_dir: Path) -> Path:
    """Write ``summary.md`` to ``out_dir``.

    :param ops: Mapping of operation key to data.
    :param out_dir: Output directory.
    :returns: Path to the written file.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "summary.md"
    out_path.write_text(build_summary(ops), encoding="utf-8")
    return out_path


# ---------------------------------------------------------------------------
# Orchestration
# ---------------------------------------------------------------------------

def generate_all(results: List[BenchmarkResult], out_dir: Path, *,
                 chart: str = "line", article_mode: bool = False) -> List[Path]:
    """Generate every plot plus the summary from parsed results.

    :param results: Aggregated benchmark results.
    :param out_dir: Output directory for PNGs and ``summary.md``.
    :param chart: ``"line"`` or ``"bar"`` style for per-operation charts.
    :param article_mode: Enable Dev.to-optimized styling.
    :returns: List of all written file paths.
    """
    ops = group_operations(results)
    written: List[Path] = []
    for od in ops.values():
        written.append(plot_operation(od, out_dir, chart=chart,
                                       article_mode=article_mode))
    overall = plot_overall_speedup(ops, out_dir, article_mode=article_mode)
    if overall is not None:
        written.append(overall)
    written.append(write_summary(ops, out_dir))
    return written


def main(argv: Optional[List[str]] = None) -> int:
    """CLI entry point.

    :param argv: Optional argument vector (defaults to ``sys.argv``).
    :returns: Process exit code.
    """
    parser = argparse.ArgumentParser(
        description="Plot C vs C++ benchmark results with Matplotlib."
    )
    parser.add_argument("input", type=Path,
                        help="Benchmark results file (.csv or .json)")
    parser.add_argument("--out", type=Path, default=Path("benchmark-results/plots"),
                        help="Output directory for plots (default: benchmark-results/plots)")
    parser.add_argument("--chart", choices=["line", "bar"], default="line",
                        help="Per-operation chart style (default: line)")
    parser.add_argument("--article-mode", action="store_true",
                        help="Generate 840x420 landscape images tuned for articles")

    args = parser.parse_args(argv)

    try:
        results = load_results(args.input)
    except (FileNotFoundError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    if not results:
        print("error: no benchmark results found in input", file=sys.stderr)
        return 2

    written = generate_all(results, args.out, chart=args.chart,
                           article_mode=args.article_mode)
    for path in written:
        print(f"wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
