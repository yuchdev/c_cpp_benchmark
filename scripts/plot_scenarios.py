#!/usr/bin/env python3
"""Charts for the matrix benchmark *scenarios* and the core speedup heatmap.

The core ``plot_results`` charts show one operation at a time against matrix size.
The scenario sweeps (see ``benchmarks/matrix/benchmarks/bench_scenarios.h``) need
richer pictures, all produced here from the same :class:`plot_results.OperationData`
the rest of the plotting code uses:

====================================  ================================================
``speedup_heatmap.png``               core op x size grid, coloured by C/C++ speedup
``scenario_chain.png``                expression-chain depth sweep (speedup per N, time)
``chain_heatmap.png``                 chain speedup, matrix size x chain depth
``scenario_fixed.png``                compile-time-N sweep (speedup per op, time)
``fixed_size_heatmap.png``            fixed-size speedup, op x N
``scenario_cliff.png``                per-element time from L1-resident to DRAM-resident
``scenario_batch.png``                4x4 transform over M points
``scenario_block.png``                submatrix copy / multiply
``scenario_tri.png``                  triangular solve / symmetric rank-k update
``scenario_conv.png``                 3x3 blur timing
``conv_images.png``                   the actual blurred images (input, C, C++, difference)
``scenario_overview.png``             geometric-mean speedup of every scenario variant
====================================  ================================================

Colour convention (shared with ``overall_speedup.png``): green = C++ faster,
purple = C faster; speedup is always ``C time / C++ time``.
"""

from __future__ import annotations

import itertools
import math
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Dict, List, Optional, Sequence, Tuple

import plot_results as pr

#: Canonical order of the core matrix operations (matches ``BENCH_ALL_OPS`` in C).
CORE_OP_ORDER = ["transpose", "add", "sub", "scale", "matvec",
                 "mul", "transpose_mul", "add3", "mul_add"]

GAIN_COLOR = "#2ca02c"   # C++ faster
LOSS_COLOR = "#9467bd"   # C faster

#: Display order and titles of the scenarios (also the order in ``summary.md``).
SCENARIO_TITLES = {
    "matrix_chain": "Expression chain  A1 + A2 + … + Ak",
    "matrix_fixed": "Compile-time-sized matrices (N = 2..16)",
    "matrix_cliff": "Cache-cliff sweep",
    "matrix_batch": "4x4 transform over M points",
    "matrix_block": "Submatrix block copy / multiply",
    "matrix_tri": "Triangular solve / symmetric rank-k update",
    "matrix_conv": "3x3 convolution (blur)",
}


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------

def _plt():
    """Import pyplot with the head-less backend (deferred so importing is cheap)."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    return plt


def speedup_by_size(od: pr.OperationData) -> Dict[float, float]:
    """Return ``{size: C time / C++ time}`` for the sizes both languages measured.

    :param od: Operation data bundle.
    :returns: Mapping from sweep parameter to speedup (non-positive times skipped).
    """
    c = od.series.get(pr.LANG_C, {})
    cpp = od.series.get(pr.LANG_CPP, {})
    return {x: c[x] / cpp[x] for x in sorted(set(c) & set(cpp))
            if c[x] > 0 and cpp[x] > 0}


def geomean(values: Sequence[float]) -> Optional[float]:
    """Geometric mean of positive ``values`` (the right average for ratios).

    :param values: Positive numbers.
    :returns: The geometric mean, or ``None`` when ``values`` is empty.
    """
    vals = [v for v in values if v > 0]
    if not vals:
        return None
    return math.exp(sum(math.log(v) for v in vals) / len(vals))


def format_bytes(n: float) -> str:
    """Render a byte count compactly (``"6KB"``, ``"1.5MB"``).

    :param n: Size in bytes.
    :returns: Human-readable size using binary prefixes.
    """
    for unit, scale in (("GB", 1 << 30), ("MB", 1 << 20), ("KB", 1 << 10)):
        if n >= scale:
            v = n / scale
            return f"{v:.0f}{unit}" if v >= 10 or v == int(v) else f"{v:.1f}{unit}"
    return f"{int(n)}B"


def format_count(n: float) -> str:
    """Render a count compactly (``"1K"``, ``"64K"``, ``"1M"``).

    :param n: Number of items.
    :returns: Count with a K/M suffix when it divides evenly.
    """
    n = int(round(n))
    if n >= 1_000_000 and n % 1_000_000 == 0:
        return f"{n // 1_000_000}M"
    if n >= 1 << 20 and n % (1 << 20) == 0:
        return f"{n >> 20}M"
    if n >= 1 << 10 and n % (1 << 10) == 0:
        return f"{n >> 10}K"
    return str(n)


def _speedup_label(v: float) -> str:
    """Format a speedup for a heatmap cell or tick."""
    return f"{v:.0f}" if v >= 10 else f"{v:.1f}"


def _tick_label(power: int) -> str:
    """Colour-bar tick text for a speedup of ``2**power`` (``"4×"``, ``"1/4×"``)."""
    return f"{2 ** power:g}×" if power >= 0 else f"1/{2 ** -power:g}×"


def _scenario_ops(ops: Dict[str, pr.OperationData], group: str) -> Dict[str, pr.OperationData]:
    """Select the operations of one scenario group, keyed by benchmark (variant) name."""
    prefix = f"{group}/"
    return {name[len(prefix):]: od for name, od in ops.items() if name.startswith(prefix)}


def _plain_log_axis(ax) -> None:
    """Give a log-scaled y-axis readable ticks (1, 2, 3, 5 per decade, plain numbers).

    Matplotlib's default labels only the decades (``10^0``) and prints the rest as
    ``3x10^0``, which is unreadable on the narrow ranges these sweeps cover.
    """
    from matplotlib.ticker import FuncFormatter, LogLocator, NullLocator
    ax.yaxis.set_major_locator(LogLocator(base=10, subs=(1.0, 2.0, 3.0, 5.0)))
    ax.yaxis.set_minor_locator(NullLocator())
    ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f"{v:g}"))


def _save(fig, out_dir: Path, filename: str) -> Path:
    """Tighten, save and close ``fig``; returns the written path."""
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / filename
    fig.tight_layout()
    fig.savefig(path)
    _plt().close(fig)
    return path


# ---------------------------------------------------------------------------
# Heatmap
# ---------------------------------------------------------------------------

def plot_speedup_heatmap(grid: Sequence[Sequence[Optional[float]]],
                         row_labels: Sequence[str], col_labels: Sequence[str], *,
                         title: str, xlabel: str, ylabel: str,
                         out_dir: Path, filename: str,
                         figsize: Optional[Tuple[float, float]] = None,
                         note: Optional[str] = None) -> Path:
    """Draw a diverging heatmap of speedup ratios (``C time / C++ time``).

    The colour scale is symmetric on a log2 axis centred on 1x, so "twice as fast"
    and "half as fast" get equally strong, opposite colours (green = C++ faster,
    purple = C faster).  Missing cells (``None``) are drawn grey.

    :param grid: ``grid[row][col]`` speedups, ``None`` for missing measurements.
    :param row_labels: One label per row.
    :param col_labels: One label per column.
    :param title: Figure title.
    :param xlabel: X-axis label.
    :param ylabel: Y-axis label.
    :param out_dir: Output directory.
    :param filename: PNG file name.
    :param figsize: Optional figure size in inches (auto-sized by default).
    :param note: Optional footnote drawn under the plot (e.g. what a grey cell means).
    :returns: Path of the written PNG.
    """
    import numpy as np
    plt = _plt()

    data = np.array([[math.log2(v) if v and v > 0 else np.nan for v in row] for row in grid],
                    dtype=float)
    finite = data[np.isfinite(data)]
    span = max(1, min(4, math.ceil(float(np.max(np.abs(finite))))) if finite.size else 1)

    cmap = plt.get_cmap("PRGn").copy()
    cmap.set_bad("#dcdcdc")
    rows, cols = data.shape
    if figsize is None:
        figsize = (max(6.0, 0.62 * cols + 3.0), max(3.2, 0.5 * rows + 2.2))
    fig, ax = plt.subplots(figsize=figsize, dpi=110)
    im = ax.imshow(np.ma.masked_invalid(data), cmap=cmap, vmin=-span, vmax=span, aspect="auto")

    ax.set_xticks(range(cols))
    ax.set_xticklabels(col_labels, fontsize=9)
    ax.set_yticks(range(rows))
    ax.set_yticklabels(row_labels, fontsize=9)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title, fontsize=13)

    norm = plt.Normalize(vmin=-span, vmax=span)
    font = 9 if cols <= 9 else 7.5
    for r in range(rows):
        for c in range(cols):
            if not np.isfinite(data[r, c]):
                continue
            red, green, blue, _ = cmap(norm(data[r, c]))
            luminance = 0.2126 * red + 0.7152 * green + 0.0722 * blue
            ax.text(c, r, _speedup_label(grid[r][c]), ha="center", va="center", fontsize=font,
                    color="black" if luminance > 0.5 else "white")

    cbar = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.03)
    ticks = list(range(-span, span + 1))
    cbar.set_ticks(ticks)
    cbar.set_ticklabels([_tick_label(t) for t in ticks])
    cbar.set_label("C++ speedup  (C time / C++ time)")
    if note:
        fig.text(0.01, 0.01, note, fontsize=8, color="#555555", ha="left", va="bottom")
    fig.tight_layout(rect=(0, 0.04 if note else 0, 1, 1))
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / filename
    fig.savefig(path)
    plt.close(fig)
    return path


def _grid_from(od_by_row: Sequence[Optional[pr.OperationData]],
               cols: Sequence[float]) -> List[List[Optional[float]]]:
    """Build ``grid[row][col]`` speedups from one operation per row."""
    grid: List[List[Optional[float]]] = []
    for od in od_by_row:
        ratios = speedup_by_size(od) if od is not None else {}
        grid.append([ratios.get(x) for x in cols])
    return grid


def plot_core_heatmap(ops: Dict[str, pr.OperationData], out_dir: Path) -> Optional[Path]:
    """Heatmap of the core matrix suite: operation x matrix size.

    :param ops: All operations (scenario ones are ignored).
    :param out_dir: Output directory.
    :returns: Path to ``speedup_heatmap.png``, or ``None`` with fewer than 2
        operations or no common size.
    """
    core = {name: od for name, od in ops.items()
            if od.suite == "matrix" and not pr.is_scenario_operation(name)}
    names = [n for n in CORE_OP_ORDER if n in core] + sorted(n for n in core if n not in CORE_OP_ORDER)
    cols = sorted({x for n in names for x in speedup_by_size(core[n])})
    if len(names) < 2 or not cols:
        return None
    return plot_speedup_heatmap(
        _grid_from([core[n] for n in names], cols), names,
        [pr.humanize_size(x, "matrix") for x in cols],
        title="C++ speedup over C: every matrix operation at every size",
        xlabel="Matrix size", ylabel="Operation", out_dir=out_dir, filename="speedup_heatmap.png",
        note="Grey cell: no valid measurement (a time at or below the clock resolution).")


# ---------------------------------------------------------------------------
# Generic sweep figure: normalised time on top, speedup underneath
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class SweepSpec:
    """How one scenario is drawn by :func:`plot_sweep`."""

    group: str
    title: str
    xlabel: str
    filename: str
    #: ``(variant, x) -> divisor`` turning raw time into time per unit of work.
    divisor: Callable[[str, float], float]
    #: ``variant -> y-axis label`` of the time panel.
    ylabel: Callable[[str], str]
    #: ``variant -> subplot title``.
    variant_title: Callable[[str], str]
    #: ``(variant, x) -> x tick label``.
    xtick: Callable[[str, float], str]


def _base_variant(variant: str) -> str:
    """Strip a trailing ``_n<param>`` from a variant name (``mul_n512`` -> ``mul``)."""
    return re.sub(r"_n\d+$", "", variant)


def variant_sort_key(variant: str) -> Tuple[int, str, int]:
    """Order variants canonically: core-op order first, then name, then numeric ``_n<param>``.

    Plain string sorting would put ``add_n128`` before ``add_n32``.

    :param variant: Variant (benchmark) name such as ``"add_n32"`` or ``"transpose_mul"``.
    :returns: A sort key.
    """
    base = _base_variant(variant)
    rank = CORE_OP_ORDER.index(base) if base in CORE_OP_ORDER else len(CORE_OP_ORDER)
    return (rank, base, _variant_param(variant) or 0)


def _variant_param(variant: str) -> Optional[int]:
    """Return the ``<param>`` of a trailing ``_n<param>`` (``mul_n512`` -> 512)."""
    m = re.search(r"_n(\d+)$", variant)
    return int(m.group(1)) if m else None


_CLIFF_MATRICES = {"add": 3, "transpose": 2, "matvec": 1}  # N x N operands touched per call
_CLIFF_TITLES = {"add": "A + B", "transpose": "Aᵀ  (transpose)", "matvec": "A · x  (matvec)"}
_BLOCK_TITLES = {"copy": "block copy", "mul": "block multiply (GEMM)"}
_TRI_TITLES = {"trsm": "triangular solve  L·X = B", "syrk": "rank-k update  lower(A·Aᵀ)"}

SWEEPS: Dict[str, SweepSpec] = {
    "matrix_cliff": SweepSpec(
        group="matrix_cliff",
        title="Cache cliff: time per element from L1-resident to DRAM-resident matrices",
        xlabel="Matrix side N  (label: total bytes touched per call)",
        filename="scenario_cliff.png",
        divisor=lambda v, x: x * x,
        ylabel=lambda v: "ns per element",
        variant_title=lambda v: _CLIFF_TITLES.get(v, v),
        xtick=lambda v, x: f"{int(x)}\n{format_bytes(_CLIFF_MATRICES.get(v, 2) * x * x * 8)}",
    ),
    "matrix_batch": SweepSpec(
        group="matrix_batch",
        title="One 4x4 transform applied to M points (Eigen maps the C buffer in place)",
        xlabel="Points M  (label: bytes read + written)",
        filename="scenario_batch.png",
        divisor=lambda v, x: x,
        ylabel=lambda v: "ns per point",
        variant_title=lambda v: "y = T · x   for every point",
        xtick=lambda v, x: f"{format_count(x)}\n{format_bytes(x * 64)}",
    ),
    "matrix_block": SweepSpec(
        group="matrix_block",
        title="Submatrix blocks inside 512x512 matrices (leading dimension ≠ block size)",
        xlabel="Block size bs",
        filename="scenario_block.png",
        divisor=lambda v, x: x ** 3 if _base_variant(v) == "mul" else x ** 2,
        ylabel=lambda v: "ns per multiply-add" if _base_variant(v) == "mul" else "ns per element",
        variant_title=lambda v: _BLOCK_TITLES.get(_base_variant(v), v),
        xtick=lambda v, x: f"{int(x)}",
    ),
    "matrix_tri": SweepSpec(
        group="matrix_tri",
        title="Triangular solve and symmetric rank-k update: only half the matrix matters",
        xlabel="Matrix size n",
        filename="scenario_tri.png",
        divisor=lambda v, x: x ** 3,
        ylabel=lambda v: "ns per n³",
        variant_title=lambda v: _TRI_TITLES.get(v, v),
        xtick=lambda v, x: f"{int(x)}",
    ),
    "matrix_conv": SweepSpec(
        group="matrix_conv",
        title="3x3 Gaussian blur of a generated image",
        xlabel="Image side N",
        filename="scenario_conv.png",
        divisor=lambda v, x: x * x,
        ylabel=lambda v: "ns per pixel",
        variant_title=lambda v: "3x3 convolution",
        xtick=lambda v, x: f"{int(x)}",
    ),
}


def _draw_speedup_panel(ax, xs: Sequence[float], ratios: Dict[float, float]) -> None:
    """Draw the speedup-vs-sweep panel (log y, 1x reference, green/purple fills)."""
    idx = {x: i for i, x in enumerate(xs)}
    pts = [(idx[x], r) for x, r in ratios.items() if x in idx]
    pts.sort()
    if not pts:
        return
    px, py = [p[0] for p in pts], [p[1] for p in pts]
    ax.set_yscale("log")
    ax.axhline(1.0, color="gray", linestyle="--", linewidth=0.9)
    ax.fill_between(px, 1.0, py, where=[v >= 1.0 for v in py], interpolate=True,
                    color=GAIN_COLOR, alpha=0.25)
    ax.fill_between(px, 1.0, py, where=[v < 1.0 for v in py], interpolate=True,
                    color=LOSS_COLOR, alpha=0.30)
    ax.plot(px, py, marker="o", color="#333333", linewidth=1.4, markersize=4)
    if len(pts) <= 10:
        for x, v in pts:
            ax.annotate(f"{v:.1f}×", (x, v), textcoords="offset points", xytext=(0, 7),
                        ha="center", fontsize=8)
    lo, hi = min(min(py), 1.0), max(max(py), 1.0)
    ax.set_ylim(lo / 1.5, hi * 1.5)
    ticks = [t for t in (1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 32) if lo / 1.5 <= t <= hi * 1.5]
    ax.set_yticks(ticks)
    ax.set_yticklabels([f"{t:g}×" for t in ticks])
    ax.minorticks_off()
    ax.set_ylabel("speedup (C / C++)")
    ax.grid(True, alpha=0.4)


def plot_sweep(spec: SweepSpec, variants: Dict[str, pr.OperationData],
               out_dir: Path) -> Optional[Path]:
    """Render one scenario: per variant, normalised time (top) and speedup (bottom).

    :param spec: Drawing instructions for the scenario.
    :param variants: ``{variant: OperationData}`` for the scenario's group.
    :param out_dir: Output directory.
    :returns: Path of the written PNG, or ``None`` when there is no data.
    """
    plt = _plt()
    names = sorted(variants, key=variant_sort_key)
    if not names:
        return None
    ncol = len(names)
    fig, axes = plt.subplots(2, ncol, figsize=(max(6.4, 5.0 * ncol), 7.2), dpi=110,
                             squeeze=False, gridspec_kw={"height_ratios": [3, 2]})
    for col, name in enumerate(names):
        od = variants[name]
        xs = od.sizes
        top, bottom = axes[0][col], axes[1][col]
        ratios = speedup_by_size(od)
        gm = geomean(list(ratios.values()))
        for lang in (pr.LANG_C, pr.LANG_CPP):
            points = od.series.get(lang)
            if not points:
                continue
            style = pr._STYLE[lang]
            ys = [points[x] * 1e9 / spec.divisor(name, x) if x in points else float("nan")
                  for x in xs]
            top.plot(range(len(xs)), ys, marker=style["marker"], color=style["color"],
                     linewidth=1.8, markersize=5, label=pr.legend_label(lang, gm))
        top.set_yscale("log")
        _plain_log_axis(top)
        top.set_title(spec.variant_title(name), fontsize=11)
        top.set_ylabel(spec.ylabel(name))
        top.grid(True, alpha=0.4)
        top.legend(fontsize=9)
        top.set_xticks(range(len(xs)))
        top.set_xticklabels([])
        _draw_speedup_panel(bottom, xs, ratios)
        bottom.set_xticks(range(len(xs)))
        bottom.set_xticklabels([spec.xtick(name, x) for x in xs],
                               fontsize=8 if len(xs) > 6 else 9)
        bottom.set_xlabel(spec.xlabel)
    fig.suptitle(spec.title, fontsize=13)
    fig.tight_layout(rect=(0, 0, 1, 0.97))
    out_dir.mkdir(parents=True, exist_ok=True)
    path = out_dir / spec.filename
    fig.savefig(path)
    plt.close(fig)
    return path


# ---------------------------------------------------------------------------
# Chain and fixed-size scenarios (two-dimensional sweeps)
# ---------------------------------------------------------------------------

def _chain_variants(ops: Dict[str, pr.OperationData]) -> List[Tuple[int, pr.OperationData]]:
    """Chain variants ``add_n<N>`` as ``(N, data)`` sorted by N."""
    out = []
    for name, od in _scenario_ops(ops, "matrix_chain").items():
        n = _variant_param(name)
        if n is not None:
            out.append((n, od))
    return sorted(out, key=lambda t: t[0])


def plot_chain(ops: Dict[str, pr.OperationData], out_dir: Path) -> List[Path]:
    """Draw the expression-chain figures (heatmap + line chart).

    :param ops: All operations.
    :param out_dir: Output directory.
    :returns: Paths of ``chain_heatmap.png`` and ``scenario_chain.png`` (empty
        when the scenario has no data).
    """
    plt = _plt()
    sizes = _chain_variants(ops)
    if not sizes:
        return []
    ks = sorted({k for _, od in sizes for k in speedup_by_size(od)})
    if not ks:
        return []
    written = [plot_speedup_heatmap(
        _grid_from([od for _, od in reversed(sizes)], ks),
        [f"N = {n}" for n, _ in reversed(sizes)], [str(int(k)) for k in ks],
        title="Expression chain A1 + … + Ak: one fused pass vs k-1 passes with temporaries",
        xlabel="Chain length k (number of matrices summed)", ylabel="Matrix size",
        out_dir=out_dir, filename="chain_heatmap.png")]

    fig, (ax_s, ax_t) = plt.subplots(1, 2, figsize=(12.5, 4.8), dpi=110)
    cmap = plt.get_cmap("viridis")
    for i, (n, od) in enumerate(sizes):
        ratios = speedup_by_size(od)
        pts = sorted(ratios.items())
        if pts:
            ax_s.plot([p[0] for p in pts], [p[1] for p in pts], marker="o", markersize=3.5,
                      linewidth=1.6, color=cmap(i / max(1, len(sizes) - 1)),
                      label=f"N = {n}  ({format_bytes(n * n * 8)}/matrix)")
    ax_s.axhline(1.0, color="gray", linestyle="--", linewidth=0.9)
    ax_s.set_xlabel("Chain length k")
    ax_s.set_ylabel("speedup (C / C++)")
    ax_s.set_title("Fusion pays while the operands stay in cache")
    ax_s.grid(True, alpha=0.4)
    ax_s.legend(fontsize=8)

    n_ref, od_ref = sizes[min(1, len(sizes) - 1)]
    unit, scale = pr.select_unit(od_ref.max_time_sec)
    for lang in (pr.LANG_C, pr.LANG_CPP):
        points = sorted(od_ref.series.get(lang, {}).items())
        if points:
            style = pr._STYLE[lang]
            ax_t.plot([p[0] for p in points], [p[1] / scale for p in points],
                      marker=style["marker"], color=style["color"], linewidth=1.8, markersize=5,
                      label=pr.legend_label(lang, geomean(list(speedup_by_size(od_ref).values()))))
    ax_t.set_xlabel("Chain length k")
    ax_t.set_ylabel(f"Execution time ({unit})")
    ax_t.set_title(f"Time per call at N = {n_ref}")
    ax_t.grid(True, alpha=0.4)
    ax_t.legend(fontsize=9)
    fig.suptitle("Expression chain: Eigen builds one expression, C composes k-1 binary adds",
                 fontsize=13)
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    path = out_dir / "scenario_chain.png"
    fig.savefig(path)
    plt.close(fig)
    written.append(path)
    return written


def _fixed_ops_in_order(ops: Dict[str, pr.OperationData]) -> List[Tuple[str, pr.OperationData]]:
    """Fixed-size variants in canonical op order."""
    fixed = _scenario_ops(ops, "matrix_fixed")
    names = [n for n in CORE_OP_ORDER if n in fixed] + sorted(n for n in fixed if n not in CORE_OP_ORDER)
    return [(n, fixed[n]) for n in names]


def plot_fixed(ops: Dict[str, pr.OperationData], out_dir: Path) -> List[Path]:
    """Draw the compile-time-N figures (heatmap + line chart).

    :param ops: All operations.
    :param out_dir: Output directory.
    :returns: Paths of ``fixed_size_heatmap.png`` and ``scenario_fixed.png`` (empty
        when the scenario has no data).
    """
    plt = _plt()
    fixed = _fixed_ops_in_order(ops)
    cols = sorted({n for _, od in fixed for n in speedup_by_size(od)})
    if not fixed or not cols:
        return []
    written = [plot_speedup_heatmap(
        _grid_from([od for _, od in fixed], cols), [n for n, _ in fixed],
        [f"{int(c)}" for c in cols],
        title="Compile-time N x N matrices: C (macro-unrolled, -O2) vs Eigen fixed-size (-O3, native)",
        xlabel="Matrix size N", ylabel="Operation",
        out_dir=out_dir, filename="fixed_size_heatmap.png")]

    fig, (ax_s, ax_t) = plt.subplots(1, 2, figsize=(12.5, 4.8), dpi=110)
    cmap = plt.get_cmap("tab10")
    for i, (name, od) in enumerate(fixed):
        pts = sorted(speedup_by_size(od).items())
        ax_s.plot([p[0] for p in pts], [p[1] for p in pts], marker="o", markersize=3,
                  linewidth=1.2, color=cmap(i % 10), label=name, alpha=0.9)
    ax_s.set_yscale("log")
    ax_s.axhline(1.0, color="gray", linestyle="--", linewidth=0.9)
    ticks = [t for t in (0.5, 1, 2, 4, 8, 16) if ax_s.get_ylim()[0] / 1.2 <= t <= ax_s.get_ylim()[1] * 1.2]
    ax_s.set_yticks(ticks)
    ax_s.set_yticklabels([f"{t:g}×" for t in ticks])
    ax_s.minorticks_off()
    ax_s.set_xlabel("Matrix size N")
    ax_s.set_ylabel("speedup (C / C++)")
    ax_s.set_title("Speedup per operation")
    ax_s.grid(True, alpha=0.4)
    ax_s.legend(fontsize=7, ncol=2)

    ref = dict(fixed).get("mul")
    if ref is not None:
        for lang in (pr.LANG_C, pr.LANG_CPP):
            points = sorted(ref.series.get(lang, {}).items())
            if points:
                style = pr._STYLE[lang]
                ax_t.plot([p[0] for p in points], [p[1] * 1e9 for p in points],
                          marker=style["marker"], color=style["color"], linewidth=1.8,
                          markersize=5,
                          label=pr.legend_label(lang, geomean(list(speedup_by_size(ref).values()))))
        ax_t.set_yscale("log")
        _plain_log_axis(ax_t)
        ax_t.set_ylabel("Execution time (ns)")
        ax_t.set_title("N x N matrix multiply, absolute time")
        ax_t.legend(fontsize=9)
    ax_t.set_xlabel("Matrix size N")
    ax_t.grid(True, alpha=0.4)
    fig.suptitle("Compile-time-sized matrices, N = 2..16", fontsize=13)
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    path = out_dir / "scenario_fixed.png"
    fig.savefig(path)
    plt.close(fig)
    written.append(path)
    return written


# ---------------------------------------------------------------------------
# Convolution images
# ---------------------------------------------------------------------------

def read_pgm(path: Path):
    """Read an 8-bit binary (``P5``) PGM file.

    :param path: Image path.
    :returns: A ``(height, width)`` ``uint8`` NumPy array.
    :raises ValueError: If the file is not a valid 8-bit ``P5`` image.
    """
    import numpy as np
    raw = Path(path).read_bytes()
    pos, tokens = 0, []
    while len(tokens) < 4:
        while pos < len(raw) and raw[pos:pos + 1].isspace():
            pos += 1
        if pos < len(raw) and raw[pos:pos + 1] == b"#":
            while pos < len(raw) and raw[pos:pos + 1] != b"\n":
                pos += 1
            continue
        start = pos
        while pos < len(raw) and not raw[pos:pos + 1].isspace():
            pos += 1
        if start == pos:
            raise ValueError(f"truncated PGM header in {path}")
        tokens.append(raw[start:pos])
    magic, width, height, maxval = tokens[0], int(tokens[1]), int(tokens[2]), int(tokens[3])
    if magic != b"P5" or maxval != 255:
        raise ValueError(f"{path}: expected 8-bit binary PGM (P5, maxval 255)")
    pixels = np.frombuffer(raw[pos + 1:pos + 1 + width * height], dtype=np.uint8)
    if pixels.size != width * height:
        raise ValueError(f"{path}: pixel data is truncated")
    return pixels.reshape(height, width)


def plot_conv_images(images_dir: Optional[Path], out_dir: Path) -> Optional[Path]:
    """Show the actual convolution result of both languages next to the input.

    The panels are the generated input, the C blur, the C++ blur and the amplified
    absolute difference of the two outputs (an 8-bit quantisation of results that
    agree to ~1e-16, so any visible difference would be a real bug).

    :param images_dir: Directory holding ``conv_{input,output}_{c,cpp}.pgm``.
    :param out_dir: Output directory.
    :returns: Path to ``conv_images.png``, or ``None`` when the images are absent.
    """
    if images_dir is None:
        return None
    images_dir = Path(images_dir)
    needed = ["conv_input_c.pgm", "conv_output_c.pgm", "conv_output_cpp.pgm"]
    if not all((images_dir / n).exists() for n in needed):
        return None
    import numpy as np
    plt = _plt()
    src = read_pgm(images_dir / "conv_input_c.pgm")
    out_c = read_pgm(images_dir / "conv_output_c.pgm")
    out_cpp = read_pgm(images_dir / "conv_output_cpp.pgm")
    if out_c.shape != out_cpp.shape:
        return None
    diff = np.abs(out_c.astype(int) - out_cpp.astype(int))

    fig, axes = plt.subplots(1, 4, figsize=(15, 4.3), dpi=110)
    panels = [
        (src, f"Input  {src.shape[1]}×{src.shape[0]}"),
        (out_c, "3×3 Gaussian blur — C"),
        (out_cpp, "3×3 Gaussian blur — C++ (Eigen)"),
    ]
    for ax, (img, title) in zip(axes[:3], panels):
        ax.imshow(img, cmap="gray", vmin=0, vmax=255, interpolation="nearest")
        ax.set_title(title, fontsize=11)
        ax.axis("off")
    axes[3].imshow(np.clip(diff * 64, 0, 255), cmap="magma", vmin=0, vmax=255,
                   interpolation="nearest")
    differing = int(np.count_nonzero(diff))
    axes[3].set_title(f"|C − C++| × 64\n{differing} of {diff.size} pixels differ "
                      f"(max {int(diff.max())} level)", fontsize=11)
    axes[3].axis("off")
    fig.suptitle("Same input, same kernel, same pixels: both languages blur identically",
                 fontsize=13)
    return _save(fig, out_dir, "conv_images.png")


# ---------------------------------------------------------------------------
# Overview + summary
# ---------------------------------------------------------------------------

def scenario_rows(ops: Dict[str, pr.OperationData]) -> List[Tuple[str, str, pr.OperationData]]:
    """List ``(scenario group, variant, data)`` for every scenario operation, in display order."""
    rows = []
    for group in SCENARIO_TITLES:
        for variant, od in sorted(_scenario_ops(ops, group).items(),
                                  key=lambda kv: variant_sort_key(kv[0])):
            rows.append((group, variant, od))
    return rows


def plot_overview(ops: Dict[str, pr.OperationData], out_dir: Path) -> Optional[Path]:
    """Bar chart of the geometric-mean speedup of every scenario variant.

    :param ops: All operations.
    :param out_dir: Output directory.
    :returns: Path to ``scenario_overview.png``, or ``None`` without scenario data.
    """
    plt = _plt()
    entries = []
    for group, variant, od in scenario_rows(ops):
        gm = geomean(list(speedup_by_size(od).values()))
        if gm is not None:
            entries.append((f"{group[len(pr.SCENARIO_GROUP_PREFIX):]}: {variant}", gm))
    if not entries:
        return None
    fig, ax = plt.subplots(figsize=(9.0, max(3.0, 0.32 * len(entries) + 1.4)), dpi=110)
    ys = list(range(len(entries)))[::-1]
    values = [math.log2(v) for _, v in entries]
    ax.barh(ys, values, color=[GAIN_COLOR if v >= 0 else LOSS_COLOR for v in values])
    for y, (_, v) in zip(ys, entries):
        ax.text(math.log2(v) + (0.05 if v >= 1 else -0.05), y, f"{v:.2f}×",
                va="center", ha="left" if v >= 1 else "right", fontsize=8)
    ax.axvline(0, color="gray", linewidth=0.9)
    ax.set_yticks(ys)
    ax.set_yticklabels([n for n, _ in entries], fontsize=8)
    lim = max(1.0, math.ceil(max(abs(v) for v in values)))
    ax.set_xlim(-lim - 0.6, lim + 0.6)
    ticks = list(range(-int(lim), int(lim) + 1))
    ax.set_xticks(ticks)
    ax.set_xticklabels([_tick_label(t) for t in ticks])
    ax.set_xlabel("Geometric-mean speedup over the sweep  (C time / C++ time)")
    ax.set_title("Scenario overview: where C++ wins, where C holds its own")
    ax.grid(True, axis="x", alpha=0.4)
    return _save(fig, out_dir, "scenario_overview.png")


def build_scenario_summary(ops: Dict[str, pr.OperationData]) -> str:
    """Markdown section describing every scenario variant (empty without scenarios).

    :param ops: All operations.
    :returns: Markdown text, or ``""`` when there are no scenario operations.
    """
    rows = scenario_rows(ops)
    if not rows:
        return ""
    lines = ["# Scenario Results", "",
             "Speedup is C time / C++ time; the average over a sweep is geometric. "
             "Scenarios are informational: unlike the core suite they are *not* expected "
             "to favour C++ everywhere.", ""]
    for group, members in itertools.groupby(rows, key=lambda r: r[0]):
        lines += [f"## {SCENARIO_TITLES[group]}", "",
                  "| Variant | Points | Geomean | Best | Worst | Winner |",
                  "|:---|---:|---:|---:|---:|:---|"]
        for _, variant, od in members:
            ratios = speedup_by_size(od)
            if not ratios:
                lines.append(f"| {variant} | 0 | n/a | n/a | n/a | n/a |")
                continue
            gm = geomean(list(ratios.values()))
            best_x = max(ratios, key=ratios.get)
            worst_x = min(ratios, key=ratios.get)
            lines.append(
                f"| {variant} | {len(ratios)} | {pr.format_speedup(gm)} | "
                f"{pr.format_speedup(ratios[best_x])} (at {best_x:g}) | "
                f"{pr.format_speedup(ratios[worst_x])} (at {worst_x:g}) | {pr.winner_for(gm)} |")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


# ---------------------------------------------------------------------------
# Orchestration
# ---------------------------------------------------------------------------

def generate_scenario_plots(ops: Dict[str, pr.OperationData], out_dir: Path, *,
                            images_dir: Optional[Path] = None) -> List[Path]:
    """Generate every heatmap and scenario figure that has data.

    :param ops: Every operation (core and scenario) from
        :func:`plot_results.group_operations`.
    :param out_dir: Output directory.
    :param images_dir: Directory with the ``conv`` scenario's PGM images; the
        ``conv_images.png`` panel is skipped when absent.
    :returns: Paths of all written files.
    """
    written: List[Path] = []

    def add(path: Optional[Path]) -> None:
        if path is not None:
            written.append(path)

    add(plot_core_heatmap(ops, out_dir))
    written += plot_chain(ops, out_dir)
    written += plot_fixed(ops, out_dir)
    for group, spec in SWEEPS.items():
        add(plot_sweep(spec, _scenario_ops(ops, group), out_dir))
    add(plot_conv_images(images_dir, out_dir))
    add(plot_overview(ops, out_dir))
    return written
