#!/usr/bin/env python3
"""Unit tests for ``scripts/plot_results.py``.

The tests cover the acceptance criteria from the specification:

* CSV parsing (both the rich and the simple schema),
* JSON parsing,
* speedup calculation,
* display-unit selection, and
* end-to-end plot generation (``sort.png`` exists, etc.).

They depend only on Matplotlib and the standard library and run head-less via
the ``Agg`` backend, so they work on Linux, macOS and Windows.
"""

import json
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

# Make ``scripts/plot_results.py`` importable regardless of the working dir.
_SCRIPTS = Path(__file__).resolve().parent.parent / "scripts"
sys.path.insert(0, str(_SCRIPTS))

import plot_results as pr  # noqa: E402


class TestUnitSelection(unittest.TestCase):
    """Verify the ns / µs / ms / s selection thresholds."""

    def test_nanoseconds(self):
        label, scale = pr.select_unit(500e-9)  # 500 ns
        self.assertEqual(label, "ns")
        self.assertAlmostEqual(scale, 1e-9)

    def test_microseconds(self):
        label, _ = pr.select_unit(500e-6)  # 500 µs
        self.assertEqual(label, "µs")

    def test_milliseconds(self):
        label, _ = pr.select_unit(0.5)  # 500 ms
        self.assertEqual(label, "ms")

    def test_seconds(self):
        label, scale = pr.select_unit(12.0)
        self.assertEqual(label, "s")
        self.assertEqual(scale, 1.0)

    def test_boundary_just_below_one_ms(self):
        # 999 µs -> still µs
        self.assertEqual(pr.select_unit(999e-6)[0], "µs")


class TestSpeedup(unittest.TestCase):
    """Verify speedup arithmetic and formatting."""

    def test_simple_ratio(self):
        self.assertEqual(1000 / 500, 2.0)

    def test_operation_average_speedup(self):
        od = pr.OperationData(
            operation="sort",
            suite="generic",
            series={
                pr.LANG_C: {1.0: 1000e-9, 2.0: 2000e-9},
                pr.LANG_CPP: {1.0: 500e-9, 2.0: 500e-9},
            },
        )
        # ratios: 2.0 and 4.0 -> mean 3.0
        self.assertEqual(od.speedups(), [2.0, 4.0])
        self.assertAlmostEqual(od.average_speedup(), 3.0)
        self.assertEqual(pr.winner_for(od.average_speedup()), "C++")

    def test_format(self):
        self.assertEqual(pr.format_speedup(2.305), "2.31×")
        self.assertEqual(pr.format_speedup(15.07), "15.07×")

    def test_legend_label_faster_and_slower(self):
        self.assertEqual(pr.legend_label(pr.LANG_C, 2.0), "C")
        self.assertEqual(pr.legend_label(pr.LANG_CPP, 2.0), "C++ (2.00× faster)")
        self.assertEqual(pr.legend_label(pr.LANG_CPP, 0.5), "C++ (2.00× slower)")


class TestCsvParsingRich(unittest.TestCase):
    """Rich schema parsing (runs.csv layout)."""

    CSV = (
        "suite,group,benchmark,language,run,measure,metric,value,unit\n"
        "generic,a,a_qsort_c,c,1,1000,wall_time_sec,0.002,sec\n"
        "generic,a,a_qsort_c,c,2,1000,wall_time_sec,0.004,sec\n"
        "generic,a,a_std_sort_cpp,cpp,1,1000,wall_time_sec,0.001,sec\n"
    )

    def test_parse_and_average(self):
        with TemporaryDirectory() as d:
            path = Path(d) / "runs.csv"
            path.write_text(self.CSV, encoding="utf-8")
            results = pr.load_results(path)

        ops = pr.group_operations(results)
        self.assertIn("sort", ops)  # group "a" mapped to friendly "sort"
        sort = ops["sort"]
        # C runs averaged: (0.002 + 0.004)/2 = 0.003 s
        self.assertAlmostEqual(sort.series[pr.LANG_C][1000.0], 0.003)
        self.assertAlmostEqual(sort.series[pr.LANG_CPP][1000.0], 0.001)
        self.assertAlmostEqual(sort.average_speedup(), 3.0)


class TestCsvParsingSimple(unittest.TestCase):
    """Simple schema parsing (implementation,operation,size,time_ns)."""

    CSV = (
        "implementation,operation,size,time_ns\n"
        "c,sort,1000,1000\n"
        "cpp,sort,1000,500\n"
    )

    def test_parse_simple(self):
        with TemporaryDirectory() as d:
            path = Path(d) / "bench.csv"
            path.write_text(self.CSV, encoding="utf-8")
            results = pr.load_results(path)

        self.assertEqual(len(results), 2)
        ops = pr.group_operations(results)
        self.assertIn("sort", ops)
        sort = ops["sort"]
        # 1000 ns normalized to seconds.
        self.assertAlmostEqual(sort.series[pr.LANG_C][1000.0], 1000e-9)
        self.assertAlmostEqual(sort.average_speedup(), 2.0)


class TestJsonParsing(unittest.TestCase):
    """JSON parsing for both list and {"results": [...]} layouts."""

    DATA = [
        {"suite": "matrix", "group": "matrix", "benchmark": "mul",
         "language": "c", "measure": 32, "metric": "avg_ns",
         "value": 5000.0, "unit": "ns", "samples": 5},
        {"suite": "matrix", "group": "matrix", "benchmark": "mul",
         "language": "cpp", "measure": 32, "metric": "avg_ns",
         "value": 1000.0, "unit": "ns", "samples": 5},
    ]

    def test_parse_list(self):
        with TemporaryDirectory() as d:
            path = Path(d) / "runs.json"
            path.write_text(json.dumps(self.DATA), encoding="utf-8")
            results = pr.load_results(path)

        ops = pr.group_operations(results)
        # Matrix benchmarks share names across languages -> split per benchmark.
        self.assertIn("mul", ops)
        self.assertAlmostEqual(ops["mul"].average_speedup(), 5.0)

    def test_parse_wrapped(self):
        with TemporaryDirectory() as d:
            path = Path(d) / "runs.json"
            path.write_text(json.dumps({"results": self.DATA}), encoding="utf-8")
            results = pr.load_results(path)
        self.assertEqual(len(results), 2)


class TestPlotGeneration(unittest.TestCase):
    """End-to-end: plots and summary.md are produced."""

    CSV = (
        "suite,group,benchmark,language,run,measure,metric,value,unit\n"
        "generic,a,a_qsort_c,c,1,1000,wall_time_sec,0.002,sec\n"
        "generic,a,a_std_sort_cpp,cpp,1,1000,wall_time_sec,0.001,sec\n"
        "generic,a,a_qsort_c,c,1,10000,wall_time_sec,0.02,sec\n"
        "generic,a,a_std_sort_cpp,cpp,1,10000,wall_time_sec,0.01,sec\n"
    )

    def test_generate_all(self):
        with TemporaryDirectory() as d:
            src = Path(d) / "runs.csv"
            src.write_text(self.CSV, encoding="utf-8")
            out = Path(d) / "plots"
            results = pr.load_results(src)
            written = pr.generate_all(results, out)

            self.assertTrue((out / "sort.png").exists())
            self.assertTrue((out / "overall_speedup.png").exists())
            self.assertTrue((out / "summary.md").exists())
            self.assertIn(out / "sort.png", written)

            summary = (out / "summary.md").read_text(encoding="utf-8")
            self.assertIn("# Benchmark Summary", summary)
            self.assertIn("Winner:", summary)

    def test_article_mode_dimensions(self):
        with TemporaryDirectory() as d:
            src = Path(d) / "runs.csv"
            src.write_text(self.CSV, encoding="utf-8")
            out = Path(d) / "plots"
            results = pr.load_results(src)
            pr.generate_all(results, out, article_mode=True)

            import struct
            raw = (out / "sort.png").read_bytes()[:33]
            width, height = struct.unpack(">II", raw[16:24])
            self.assertEqual((width, height), (840, 420))


if __name__ == "__main__":
    unittest.main()
