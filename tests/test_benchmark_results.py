#!/usr/bin/env python3
"""Integration tests: full benchmark pipeline.

Runs ``scripts/run_all_benchmarks.py`` with both generic *and* matrix suites
enabled and verifies:

1. Every expected result file and plot image is present (runs.csv, summary.csv,
   runs.json, results_{group}.csv for groups a–e and matrix, per-operation PNGs,
   overall_speedup.png, summary.md).
2. The result CSV contains rows for every generic group (a–e) and the matrix
   suite, for both C and C++ languages.
3. At least **95 %** of paired (C, C++) comparisons by (operation, size) show
   C++ superiority (speedup = C_time / C++_time > 1.0).
4. All five generic benchmark groups individually report average speedup > 1.0.
5. Core compute-bound matrix operations (mul, matvec, transpose_mul) show
   C++ superiority at every measured size.

Benchmark sizes used here match the representative settings from README.md,
trimmed slightly for CI speed (``--sort 1000000`` instead of 10 M, ``--repeats 3``).

The test module is **skipped entirely** when the build directory is absent —
set the ``BUILD_DIR`` environment variable to override the default
(``<repo_root>/cmake-build``).

These tests take ~30–90 s and require:
* Built benchmark executables (see Build Instructions in README.md).
* Matplotlib (``pip install matplotlib``).
"""

from __future__ import annotations

import csv
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import List, Optional, Tuple

_REPO_ROOT = Path(__file__).resolve().parent.parent
_SCRIPTS = _REPO_ROOT / "scripts"
sys.path.insert(0, str(_SCRIPTS))

import plot_results as pr  # noqa: E402  (requires _SCRIPTS on path)

# ---------------------------------------------------------------------------
# Module-level shared state — pipeline runs once, all test classes share it.
# ---------------------------------------------------------------------------

_tmpdir_obj: Optional[tempfile.TemporaryDirectory] = None
_results_dir: Optional[Path] = None


def _build_dir() -> Path:
    return Path(os.environ.get("BUILD_DIR", _REPO_ROOT / "cmake-build")).resolve()


def setUpModule() -> None:  # noqa: N802 (unittest hook naming)
    """Build pipeline is executed once; all test classes share the results dir.

    Raises ``SkipTest`` if the build directory is absent so that the module can
    still be imported in environments where CMake has not been run.
    """
    global _tmpdir_obj, _results_dir

    bd = _build_dir()
    if not bd.exists():
        raise unittest.SkipTest(
            f"Build directory '{bd}' not found. Run:\n"
            "  cmake -S . -B cmake-build -DCMAKE_BUILD_TYPE=Release\n"
            "  cmake --build cmake-build -j4\n"
            "or set the BUILD_DIR environment variable."
        )

    _tmpdir_obj = tempfile.TemporaryDirectory()
    _results_dir = Path(_tmpdir_obj.name)
    plots_dir = _results_dir / "plots"

    cmd = [
        sys.executable,
        str(_SCRIPTS / "run_all_benchmarks.py"),
        "--build-dir", str(bd),
        "--output-dir", str(_results_dir),
        "--repeats", "3",
        # Generic benchmark sizes — representative but CI-friendly
        "--sort",       "1000000",
        "--callback",   "10000000",
        "--struct-api", "10000000",
        "--buffer",     "262144",
        "--table",      "10000000",
        # Generate Matplotlib charts into plots/
        "--plot",
        "--plots-dir", str(plots_dir),
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"run_all_benchmarks.py exited with code {result.returncode}.\n"
            f"--- STDOUT (last 4000 chars) ---\n{result.stdout[-4000:]}\n"
            f"--- STDERR (last 4000 chars) ---\n{result.stderr[-4000:]}"
        )


def tearDownModule() -> None:  # noqa: N802
    global _tmpdir_obj, _results_dir
    if _tmpdir_obj is not None:
        _tmpdir_obj.cleanup()
        _tmpdir_obj = None
        _results_dir = None


def _dir() -> Path:
    """Return the shared results directory, asserting it is initialised."""
    assert _results_dir is not None, "setUpModule did not initialise _results_dir"
    return _results_dir


# ---------------------------------------------------------------------------
# Helper: load paired speedups from runs.json
# ---------------------------------------------------------------------------

def _paired_speedups() -> List[Tuple[str, float, float]]:
    """Return ``(operation, size, speedup)`` for every matched C/C++ pair."""
    results = pr.load_results(_dir() / "runs.json")
    ops = pr.group_operations(results)
    out: List[Tuple[str, float, float]] = []
    for op_name, op_data in ops.items():
        c_series = op_data.series.get(pr.LANG_C, {})
        cpp_series = op_data.series.get(pr.LANG_CPP, {})
        for size, c_time in c_series.items():
            cpp_time = cpp_series.get(size)
            if cpp_time and cpp_time > 0 and c_time > 0:
                out.append((op_name, size, c_time / cpp_time))
    return out


# ===========================================================================
# Test class 1 — output files
# ===========================================================================

class TestOutputFiles(unittest.TestCase):
    """Every expected result file and plot image must be produced."""

    # ── Tabular result files ────────────────────────────────────────────────

    def test_runs_csv_exists(self):
        self.assertTrue((_dir() / "runs.csv").exists())

    def test_summary_csv_exists(self):
        self.assertTrue((_dir() / "summary.csv").exists())

    def test_runs_json_exists(self):
        self.assertTrue((_dir() / "runs.json").exists())

    def test_runs_csv_non_empty(self):
        with (_dir() / "runs.csv").open(encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        self.assertGreater(len(rows), 0, "runs.csv must contain at least one data row")

    def test_runs_json_schema(self):
        with (_dir() / "runs.json").open(encoding="utf-8") as f:
            data = json.load(f)
        self.assertIsInstance(data, list)
        self.assertGreater(len(data), 0, "runs.json must not be empty")
        required = {"suite", "group", "benchmark", "language", "measure", "metric", "value", "unit"}
        for entry in data[:5]:
            missing = required - entry.keys()
            self.assertFalse(missing, f"runs.json entry is missing fields: {missing}")

    def test_summary_csv_has_stat_columns(self):
        with (_dir() / "summary.csv").open(encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        self.assertGreater(len(rows), 0)
        for col in ("mean", "min", "max", "stdev", "samples"):
            self.assertIn(col, rows[0], f"summary.csv is missing column '{col}'")

    # ── Per-group CSVs (generic a–e + matrix) ───────────────────────────────

    def test_group_csv_a(self):
        self.assertTrue((_dir() / "results_a.csv").exists())

    def test_group_csv_b(self):
        self.assertTrue((_dir() / "results_b.csv").exists())

    def test_group_csv_c(self):
        self.assertTrue((_dir() / "results_c.csv").exists())

    def test_group_csv_d(self):
        self.assertTrue((_dir() / "results_d.csv").exists())

    def test_group_csv_e(self):
        self.assertTrue((_dir() / "results_e.csv").exists())

    def test_group_csv_matrix(self):
        self.assertTrue((_dir() / "results_matrix.csv").exists())

    # ── Generic per-operation PNGs ───────────────────────────────────────────

    def test_png_sort(self):
        self.assertTrue((_dir() / "plots" / "sort.png").exists())

    def test_png_callback(self):
        self.assertTrue((_dir() / "plots" / "callback.png").exists())

    def test_png_struct_api(self):
        self.assertTrue((_dir() / "plots" / "struct_api.png").exists())

    def test_png_copy_move(self):
        self.assertTrue((_dir() / "plots" / "copy_move.png").exists())

    def test_png_lookup_table(self):
        self.assertTrue((_dir() / "plots" / "lookup_table.png").exists())

    # ── Matrix per-operation PNGs ────────────────────────────────────────────

    def test_png_matrix_mul(self):
        self.assertTrue((_dir() / "plots" / "mul.png").exists())

    def test_png_matrix_add(self):
        self.assertTrue((_dir() / "plots" / "add.png").exists())

    def test_png_matrix_matvec(self):
        self.assertTrue((_dir() / "plots" / "matvec.png").exists())

    def test_png_matrix_transpose(self):
        self.assertTrue((_dir() / "plots" / "transpose.png").exists())

    # ── Aggregate summary outputs ────────────────────────────────────────────

    def test_overall_speedup_png(self):
        self.assertTrue((_dir() / "plots" / "overall_speedup.png").exists())

    def test_summary_md_exists(self):
        self.assertTrue((_dir() / "plots" / "summary.md").exists())

    def test_summary_md_content(self):
        content = (_dir() / "plots" / "summary.md").read_text(encoding="utf-8")
        self.assertIn("# Benchmark Summary", content)
        self.assertIn("Winner:", content)


# ===========================================================================
# Test class 2 — result content
# ===========================================================================

class TestResultContent(unittest.TestCase):
    """Data inside the result files must be complete and well-structured."""

    def test_all_generic_groups_in_runs_csv(self):
        with (_dir() / "runs.csv").open(encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        groups = {r["group"] for r in rows if r["suite"] == "generic"}
        for g in ("a", "b", "c", "d", "e"):
            self.assertIn(g, groups, f"Generic group '{g}' missing from runs.csv")

    def test_matrix_suite_in_runs_csv(self):
        with (_dir() / "runs.csv").open(encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        matrix_rows = [r for r in rows if r["suite"] == "matrix"]
        self.assertGreater(len(matrix_rows), 0, "No matrix rows in runs.csv")

    def test_both_languages_in_runs_csv(self):
        with (_dir() / "runs.csv").open(encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        langs = {r["language"] for r in rows}
        self.assertIn("c", langs, "No C rows in runs.csv")
        self.assertIn("cpp", langs, "No C++ rows in runs.csv")

    def test_matrix_core_operations_present(self):
        with (_dir() / "runs.csv").open(encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        ops = {r["benchmark"] for r in rows if r["suite"] == "matrix"}
        for op in ("mul", "add", "matvec", "transpose"):
            self.assertIn(op, ops, f"Matrix operation '{op}' missing from runs.csv")

    def test_generic_values_are_positive(self):
        with (_dir() / "runs.csv").open(encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        generic = [r for r in rows if r["suite"] == "generic"]
        self.assertGreater(len(generic), 0)
        for r in generic:
            val = float(r["value"])
            self.assertGreater(val, 0, f"Non-positive timing in row: {r}")

    def test_matrix_values_are_positive(self):
        with (_dir() / "runs.csv").open(encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        matrix = [r for r in rows if r["suite"] == "matrix"]
        self.assertGreater(len(matrix), 0)
        for r in matrix:
            val = float(r["value"])
            self.assertGreater(val, 0, f"Non-positive timing in row: {r}")


# ===========================================================================
# Test class 3 — C++ superiority
# ===========================================================================

class TestCppSuperiority(unittest.TestCase):
    """C++ must demonstrate measurable performance advantages.

    Three checks at increasing specificity:

    1. **Overall 95 %**: at least 19 out of every 20 paired (operation, size)
       comparisons must show speedup > 1.0.  The 5 % tolerance accounts for
       documented element-wise matrix operations (add/sub/scale at large sizes)
       that are memory-bandwidth-bound and approach parity.

    2. **Generic 100 %**: every generic group (sort, callback, struct_api,
       copy_move, lookup_table) must individually show average speedup > 1.0.
       These isolate structural compiler advantages and should never regress.

    3. **Compute-bound matrix 100 %**: mul, matvec, and transpose_mul must show
       speedup > 1.0 at every benchmarked size — the 5–10× Eigen advantage here
       is robust to timing noise.
    """

    def test_overall_95_percent_cpp_superior(self):
        speedups = _paired_speedups()
        self.assertGreater(len(speedups), 0, "No paired C/C++ comparisons found in results")

        superior = sum(1 for _, _, s in speedups if s > 1.0)
        pct = superior / len(speedups)

        non_superior = sorted(
            ((op, int(sz), s) for op, sz, s in speedups if s <= 1.0),
            key=lambda t: (t[0], t[1]),
        )
        detail = "\n".join(
            f"  {op:22s}  size={sz:>8d}  ratio={s:.3f}×"
            for op, sz, s in non_superior
        )
        msg = (
            f"C++ superior in {superior}/{len(speedups)} comparisons ({pct:.1%}); "
            f"threshold 95 %.\n"
            f"Non-superior pairs (documented parity cases are expected here):\n"
            f"{detail or '  (none)'}"
        )
        self.assertGreaterEqual(pct, 0.95, msg)

    def test_all_generic_groups_cpp_superior(self):
        results = pr.load_results(_dir() / "runs.json")
        ops = pr.group_operations(results)

        for label in pr.GROUP_LABELS.values():   # sort, callback, struct_api, copy_move, lookup_table
            if label not in ops:
                self.fail(f"Generic operation '{label}' not found in results")
            speedup = ops[label].average_speedup()
            self.assertGreater(
                speedup, 1.0,
                f"Generic '{label}': average speedup {speedup:.3f}× — C++ must win",
            )

    def test_matrix_compute_ops_cpp_superior_all_sizes(self):
        results = pr.load_results(_dir() / "runs.json")
        ops = pr.group_operations(results)

        for op_name in ("mul", "matvec", "transpose_mul"):
            if op_name not in ops:
                continue  # skip if not produced (e.g. matrix suite disabled)
            op_data = ops[op_name]
            c_series = op_data.series.get(pr.LANG_C, {})
            cpp_series = op_data.series.get(pr.LANG_CPP, {})
            for size in sorted(c_series):
                cpp_time = cpp_series.get(size)
                if cpp_time is None:
                    continue
                speedup = c_series[size] / cpp_time
                self.assertGreater(
                    speedup, 1.0,
                    f"Matrix '{op_name}' at size {int(size)}: speedup {speedup:.3f}× — "
                    f"C++ must outperform C for compute-bound operations",
                )


if __name__ == "__main__":
    unittest.main()
