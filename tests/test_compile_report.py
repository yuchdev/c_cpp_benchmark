"""Tests for C/C++ speedup annotations in compile_report.py."""

import sys
import unittest
from pathlib import Path

_SCRIPTS = Path(__file__).resolve().parent.parent / "scripts"
sys.path.insert(0, str(_SCRIPTS))

import compile_report  # noqa: E402


class TestCompileReport(unittest.TestCase):
    def test_reports_how_much_cpp_is_faster(self):
        results = [
            {
                "suite": "generic",
                "group": "a",
                "benchmark": "sort_c",
                "language": "c",
                "measure": 1000,
                "metric": "wall_time_sec",
                "value": 0.004,
                "unit": "sec",
                "samples": 5,
            },
            {
                "suite": "generic",
                "group": "a",
                "benchmark": "sort_cpp",
                "language": "cpp",
                "measure": 1000,
                "metric": "wall_time_sec",
                "value": 0.001,
                "unit": "sec",
                "samples": 5,
            },
        ]

        report = compile_report.generate_md(results)

        self.assertIn("C++ vs C", report)
        self.assertIn("4.00× faster", report)

    def test_reports_when_cpp_is_slower(self):
        results = [
            {
                "suite": "matrix",
                "group": "mul",
                "benchmark": "mul",
                "language": "c",
                "measure": 8,
                "metric": "avg_ns",
                "value": 2.0,
                "unit": "ns",
                "samples": 1,
            },
            {
                "suite": "matrix",
                "group": "mul",
                "benchmark": "mul",
                "language": "cpp",
                "measure": 8,
                "metric": "avg_ns",
                "value": 4.0,
                "unit": "ns",
                "samples": 1,
            },
        ]

        report = compile_report.generate_md(results)

        self.assertIn("2.00× slower", report)

    def test_pairs_shared_group_by_benchmark_name(self):
        results = [
            {
                "suite": "matrix",
                "group": "matrix",
                "benchmark": benchmark,
                "language": language,
                "measure": 8,
                "metric": "avg_ns",
                "value": value,
                "unit": "ns",
                "samples": 1,
            }
            for benchmark, language, value in (
                ("add", "c", 6.0),
                ("add", "cpp", 2.0),
                ("mul", "c", 8.0),
                ("mul", "cpp", 4.0),
            )
        ]

        report = compile_report.generate_md(results)

        self.assertIn("3.00× faster", report)
        self.assertIn("2.00× faster", report)

    def test_does_not_compare_different_measures(self):
        results = [
            {
                "suite": "generic",
                "group": "a",
                "benchmark": "sort_c",
                "language": "c",
                "measure": 1000,
                "metric": "wall_time_sec",
                "value": 0.004,
                "unit": "sec",
                "samples": 5,
            },
            {
                "suite": "generic",
                "group": "a",
                "benchmark": "sort_cpp",
                "language": "cpp",
                "measure": 2000,
                "metric": "wall_time_sec",
                "value": 0.001,
                "unit": "sec",
                "samples": 5,
            },
        ]

        report = compile_report.generate_md(results)

        self.assertNotIn("× faster", report)
        self.assertNotIn("× slower", report)


if __name__ == "__main__":
    unittest.main()
