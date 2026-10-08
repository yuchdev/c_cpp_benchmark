#!/usr/bin/env python3
"""Unit tests for the matrix-scenario charts (``scripts/plot_scenarios.py``) and the
scenario handling in ``scripts/plot_results.py``.

Everything runs on small synthetic result sets, head-less (Agg backend), so no
benchmark has to be built or run.
"""

import math
import struct
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

_SCRIPTS = Path(__file__).resolve().parent.parent / "scripts"
sys.path.insert(0, str(_SCRIPTS))

import plot_results as pr  # noqa: E402
import plot_scenarios as ps  # noqa: E402


def _row(group, benchmark, lang, measure, ns):
    return {"suite": "matrix", "group": group, "benchmark": benchmark, "language": lang,
            "measure": measure, "value": ns, "unit": "ns"}


def _pair(rows, group, benchmark, measure, c_ns, cpp_ns):
    rows.append(_row(group, benchmark, "c", measure, c_ns))
    rows.append(_row(group, benchmark, "cpp", measure, cpp_ns))


def synthetic_rows():
    """One tiny but complete result set: core grid + every scenario group."""
    rows = []
    # core grid: 3 ops x 3 sizes (the names deliberately collide with fixed-size ops)
    for op, ratio in (("add", 1.2), ("mul", 6.0), ("matvec", 3.0)):
        for n in (32, 128, 512):
            _pair(rows, "matrix", op, n, 1000.0 * ratio * n, 1000.0 * n)
    # chain: two matrix sizes x chain depths
    for n in (32, 128):
        for k in (2, 4, 8):
            _pair(rows, "matrix_chain", f"add_n{n}", k, 100.0 * k * n, 40.0 * k * n / 2)
    # fixed: every op at N = 2, 3, 4 (same op names as the core grid!)
    for op in ("add", "mul", "transpose"):
        for n in (2, 3, 4):
            _pair(rows, "matrix_fixed", op, n, 10.0 * n, 5.0 * n)
    # cliff
    for op in ("add", "transpose", "matvec"):
        for n in (16, 64, 256):
            _pair(rows, "matrix_cliff", op, n, 2.0 * n * n, 1.0 * n * n)
    # batch
    for m in (1024, 4096):
        _pair(rows, "matrix_batch", "xform4", m, 3.0 * m, 2.0 * m)
    # block (note the _n512 suffix) with one size where C wins
    for variant in ("copy_n512", "mul_n512"):
        for bs, ratio in ((4, 0.8), (16, 2.0), (64, 5.0)):
            _pair(rows, "matrix_block", variant, bs, 100.0 * ratio * bs, 100.0 * bs)
    # tri, conv
    for variant in ("trsm", "syrk"):
        for n in (16, 64):
            _pair(rows, "matrix_tri", variant, n, 9.0 * n ** 3, 3.0 * n ** 3)
    for n in (64, 128):
        _pair(rows, "matrix_conv", "blur3x3", n, 5.0 * n * n, 8.0 * n * n)
    return rows


class TestScenarioNamespacing(unittest.TestCase):
    """Scenario rows are namespaced so they cannot merge into the core operations."""

    @classmethod
    def setUpClass(cls):
        cls.results = pr.parse_records(synthetic_rows())
        cls.ops = pr.group_operations(cls.results)

    def test_scenario_ops_are_namespaced(self):
        self.assertIn("matrix_fixed/mul", self.ops)
        self.assertIn("matrix_chain/add_n32", self.ops)
        self.assertIn("matrix_block/mul_n512", self.ops)

    def test_core_and_fixed_ops_with_the_same_name_do_not_merge(self):
        core, fixed = self.ops["mul"], self.ops["matrix_fixed/mul"]
        self.assertEqual(sorted(core.series[pr.LANG_C]), [32.0, 128.0, 512.0])
        self.assertEqual(sorted(fixed.series[pr.LANG_C]), [2.0, 3.0, 4.0])

    def test_group_is_carried_through(self):
        self.assertEqual(self.ops["matrix_fixed/mul"].group, "matrix_fixed")
        self.assertEqual(self.ops["mul"].group, "matrix")
        self.assertTrue(all(r.group for r in self.results))

    def test_predicates(self):
        self.assertTrue(pr.is_scenario_group("matrix_chain"))
        self.assertFalse(pr.is_scenario_group("matrix"))
        self.assertFalse(pr.is_scenario_group("a"))
        self.assertTrue(pr.is_scenario_operation("matrix_tri/syrk"))
        self.assertFalse(pr.is_scenario_operation("mul"))

    def test_existing_rows_without_scenarios_are_unchanged(self):
        rows = [r for r in synthetic_rows() if r["group"] == "matrix"]
        ops = pr.group_operations(pr.parse_records(rows))
        self.assertEqual(sorted(ops), ["add", "matvec", "mul"])
        self.assertAlmostEqual(ops["mul"].average_speedup(), 6.0)

    def test_simple_schema_still_has_empty_group(self):
        results = pr.parse_records([
            {"implementation": "c", "operation": "sort", "size": 10, "time_ns": 100},
            {"implementation": "cpp", "operation": "sort", "size": 10, "time_ns": 50},
        ])
        self.assertTrue(all(r.group == "" for r in results))


class TestHelpers(unittest.TestCase):
    def test_geomean(self):
        self.assertAlmostEqual(ps.geomean([2.0, 8.0]), 4.0)
        self.assertAlmostEqual(ps.geomean([1.0, 1.0, 1.0]), 1.0)
        self.assertIsNone(ps.geomean([]))
        self.assertAlmostEqual(ps.geomean([4.0, 0.0, -1.0]), 4.0)  # non-positive ignored

    def test_geomean_is_symmetric_for_ratios(self):
        # a 2x win and a 2x loss cancel out (the arithmetic mean would say 1.25x)
        self.assertAlmostEqual(ps.geomean([2.0, 0.5]), 1.0)

    def test_format_bytes(self):
        self.assertEqual(ps.format_bytes(512), "512B")
        self.assertEqual(ps.format_bytes(2048), "2KB")
        self.assertEqual(ps.format_bytes(1536), "1.5KB")
        self.assertEqual(ps.format_bytes(3 * 1024 * 1024), "3MB")
        self.assertEqual(ps.format_bytes(96 * 1024 * 1024), "96MB")

    def test_format_count(self):
        self.assertEqual(ps.format_count(1024), "1K")
        self.assertEqual(ps.format_count(65536), "64K")
        self.assertEqual(ps.format_count(1048576), "1M")
        self.assertEqual(ps.format_count(1000), "1000")

    def test_variant_sort_key_orders_numerically_and_by_canonical_op(self):
        names = ["add_n128", "add_n32", "add_n512", "add_n64", "add_n256"]
        self.assertEqual(sorted(names, key=ps.variant_sort_key),
                         ["add_n32", "add_n64", "add_n128", "add_n256", "add_n512"])
        ops = ["mul_add", "add", "transpose_mul", "transpose", "matvec"]
        self.assertEqual(sorted(ops, key=ps.variant_sort_key),
                         ["transpose", "add", "matvec", "transpose_mul", "mul_add"])

    def test_speedup_by_size_skips_nonpositive_and_one_sided_points(self):
        od = pr.OperationData("x", "matrix", {
            pr.LANG_C: {1.0: 4e-9, 2.0: 0.0, 3.0: 9e-9},
            pr.LANG_CPP: {1.0: 2e-9, 2.0: 1e-9},
        })
        self.assertEqual(ps.speedup_by_size(od), {1.0: 2.0})


def _pgm(width, height, pixels, header_comment=b""):
    return b"P5\n" + header_comment + f"{width} {height}\n255\n".encode() + bytes(pixels)


class TestReadPgm(unittest.TestCase):
    def _read(self, data):
        with TemporaryDirectory() as d:
            p = Path(d) / "x.pgm"
            p.write_bytes(data)
            return ps.read_pgm(p)

    def test_reads_pixels_and_shape(self):
        img = self._read(_pgm(3, 2, [0, 10, 20, 30, 40, 255]))
        self.assertEqual(img.shape, (2, 3))
        self.assertEqual(img.tolist(), [[0, 10, 20], [30, 40, 255]])

    def test_header_comments_are_skipped(self):
        img = self._read(_pgm(2, 1, [7, 9], header_comment=b"# made by test\n"))
        self.assertEqual(img.tolist(), [[7, 9]])

    def test_pixel_that_looks_like_whitespace_is_not_eaten(self):
        # pixel value 10 is "\n": exactly one separator byte may be skipped after maxval
        img = self._read(_pgm(2, 1, [10, 32]))
        self.assertEqual(img.tolist(), [[10, 32]])

    def test_rejects_wrong_format(self):
        with self.assertRaises(ValueError):
            self._read(b"P2\n2 1\n255\n1 2\n")
        with self.assertRaises(ValueError):
            self._read(b"P5\n2 1\n65535\n\x00\x01\x00\x02")

    def test_rejects_truncated_data(self):
        with self.assertRaises(ValueError):
            self._read(_pgm(4, 4, [1, 2, 3]))


class TestScenarioPlots(unittest.TestCase):
    """End-to-end figure generation from synthetic results."""

    @classmethod
    def setUpClass(cls):
        cls._tmp = TemporaryDirectory()
        cls.out = Path(cls._tmp.name) / "plots"
        cls.images = Path(cls._tmp.name) / "images"
        cls.images.mkdir()
        # a 64x64 input and two identical 62x62 "blurs" (what a correct run looks like)
        (cls.images / "conv_input_c.pgm").write_bytes(_pgm(64, 64, [i % 256 for i in range(64 * 64)]))
        blur = _pgm(62, 62, [(i * 3) % 256 for i in range(62 * 62)])
        (cls.images / "conv_output_c.pgm").write_bytes(blur)
        (cls.images / "conv_output_cpp.pgm").write_bytes(blur)
        results = pr.parse_records(synthetic_rows())
        cls.written = pr.generate_all(results, cls.out, images_dir=cls.images)

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_scenario_figures_exist(self):
        for name in ("speedup_heatmap.png", "scenario_chain.png", "chain_heatmap.png",
                     "scenario_fixed.png", "fixed_size_heatmap.png", "scenario_cliff.png",
                     "scenario_batch.png", "scenario_block.png", "scenario_tri.png",
                     "scenario_conv.png", "conv_images.png", "scenario_overview.png"):
            with self.subTest(name=name):
                self.assertTrue((self.out / name).exists())
                self.assertIn(self.out / name, self.written)
                self.assertGreater((self.out / name).stat().st_size, 2000)

    def test_core_charts_are_untouched_by_scenarios(self):
        for name in ("add.png", "mul.png", "matvec.png", "overall_speedup.png", "summary.md"):
            self.assertTrue((self.out / name).exists(), name)

    def test_no_per_operation_chart_for_scenario_ops(self):
        pngs = {p.name for p in self.out.glob("*.png")}
        for stray in ("matrix_fixed_mul.png", "matrix_chain_add_n32.png", "add_n32.png"):
            self.assertNotIn(stray, pngs)

    def test_png_files_are_valid_images(self):
        for p in self.out.glob("*.png"):
            self.assertEqual(p.read_bytes()[:8], b"\x89PNG\r\n\x1a\n", p.name)

    def test_summary_has_core_and_scenario_sections(self):
        text = (self.out / "summary.md").read_text(encoding="utf-8")
        self.assertIn("# Benchmark Summary", text)
        self.assertIn("# Scenario Results", text)
        for title in ps.SCENARIO_TITLES.values():
            self.assertIn(f"## {title}", text)
        self.assertIn("| add_n32 |", text)
        # core section must not list scenario operations as separate "## ..." entries
        core = text.split("# Scenario Results")[0]
        self.assertNotIn("Matrix_Fixed", core)

    def test_summary_values(self):
        text = ps.build_scenario_summary(pr.group_operations(pr.parse_records(synthetic_rows())))
        # conv: C is faster (5 vs 8 per pixel-ish) -> C wins
        conv_line = [l for l in text.splitlines() if l.startswith("| blur3x3 |")][0]
        self.assertTrue(conv_line.rstrip().endswith("| C |"), conv_line)
        # tri: 3x faster everywhere -> geomean 3.00x, C++ wins
        syrk_line = [l for l in text.splitlines() if l.startswith("| syrk |")][0]
        self.assertIn("3.00×", syrk_line)
        self.assertTrue(syrk_line.rstrip().endswith("| C++ |"))
        # block copy has a losing point (0.8x at bs=4): worst < 1
        copy_line = [l for l in text.splitlines() if l.startswith("| copy_n512 |")][0]
        self.assertIn("0.80× (at 4)", copy_line)

    def test_summary_is_empty_without_scenarios(self):
        rows = [r for r in synthetic_rows() if r["group"] == "matrix"]
        ops = pr.group_operations(pr.parse_records(rows))
        self.assertEqual(ps.build_scenario_summary(ops), "")
        self.assertNotIn("Scenario Results", pr.build_summary(ops))


class TestPartialData(unittest.TestCase):
    """Missing pieces must be skipped quietly, never crash the whole plotting run."""

    def _generate(self, rows, images_dir=None, everything=False):
        """File names written for ``rows``.

        By default only the scenario layer runs (the core per-operation charts are
        covered elsewhere and cost seconds each); ``everything=True`` runs the full
        :func:`plot_results.generate_all`.
        """
        with TemporaryDirectory() as d:
            out = Path(d) / "plots"
            results = pr.parse_records(rows)
            if everything:
                written = pr.generate_all(results, out, images_dir=images_dir)
            else:
                written = ps.generate_scenario_plots(pr.group_operations(results), out,
                                                     images_dir=images_dir)
            return {p.name for p in written}

    def test_core_only_results_produce_a_heatmap_and_no_scenario_figures(self):
        rows = [r for r in synthetic_rows() if r["group"] == "matrix"]
        names = self._generate(rows)
        self.assertIn("speedup_heatmap.png", names)
        self.assertFalse({n for n in names if n.startswith("scenario_")})

    def test_single_operation_has_no_heatmap(self):
        rows = [r for r in synthetic_rows() if r["group"] == "matrix" and r["benchmark"] == "mul"]
        self.assertNotIn("speedup_heatmap.png", self._generate(rows))

    def test_generic_only_results(self):
        rows = [
            {"suite": "generic", "group": "a", "benchmark": "a_qsort_c", "language": "c",
             "measure": 1000, "value": 0.002, "unit": "sec"},
            {"suite": "generic", "group": "a", "benchmark": "a_std_sort_cpp", "language": "cpp",
             "measure": 1000, "value": 0.001, "unit": "sec"},
        ]
        names = self._generate(rows, everything=True)
        self.assertIn("sort.png", names)
        self.assertNotIn("speedup_heatmap.png", names)
        self.assertNotIn("scenario_overview.png", names)

    def test_one_language_only_scenario_does_not_crash(self):
        rows = [_row("matrix_cliff", "add", "cpp", n, 10.0 * n) for n in (16, 32, 64)]
        names = self._generate(rows)  # no C data -> no speedups, but must not raise
        self.assertIn("scenario_cliff.png", names)
        self.assertNotIn("scenario_overview.png", names)

    def test_missing_or_incomplete_images_skip_the_panel(self):
        rows = [r for r in synthetic_rows() if r["group"] == "matrix_conv"]
        self.assertNotIn("conv_images.png", self._generate(rows, images_dir=None))
        with TemporaryDirectory() as d:
            self.assertNotIn("conv_images.png", self._generate(rows, images_dir=Path(d)))

    def test_mismatched_image_shapes_skip_the_panel(self):
        with TemporaryDirectory() as d:
            img = Path(d)
            (img / "conv_input_c.pgm").write_bytes(_pgm(8, 8, [0] * 64))
            (img / "conv_output_c.pgm").write_bytes(_pgm(6, 6, [0] * 36))
            (img / "conv_output_cpp.pgm").write_bytes(_pgm(5, 5, [0] * 25))
            self.assertIsNone(ps.plot_conv_images(img, Path(d) / "out"))


class TestHeatmap(unittest.TestCase):
    def test_handles_missing_cells_and_extreme_ratios(self):
        with TemporaryDirectory() as d:
            path = ps.plot_speedup_heatmap(
                [[1.0, None, 100.0], [0.001, 2.0, 0.5]], ["a", "b"], ["x", "y", "z"],
                title="t", xlabel="x", ylabel="y", out_dir=Path(d), filename="h.png",
                note="grey = missing")
            self.assertTrue(path.exists())

    def test_all_missing_does_not_crash(self):
        with TemporaryDirectory() as d:
            path = ps.plot_speedup_heatmap([[None, None]], ["a"], ["x", "y"], title="t",
                                           xlabel="x", ylabel="y", out_dir=Path(d),
                                           filename="h.png")
            self.assertTrue(path.exists())

    def test_png_dimensions_are_reasonable(self):
        with TemporaryDirectory() as d:
            path = ps.plot_speedup_heatmap([[1.0, 2.0], [3.0, 4.0]], ["a", "b"], ["x", "y"],
                                           title="t", xlabel="x", ylabel="y",
                                           out_dir=Path(d), filename="h.png")
            width, height = struct.unpack(">II", path.read_bytes()[16:24])
            self.assertGreater(width, 300)
            self.assertGreater(height, 200)

    def test_tick_labels(self):
        self.assertEqual(ps._tick_label(0), "1×")
        self.assertEqual(ps._tick_label(3), "8×")
        self.assertEqual(ps._tick_label(-2), "1/4×")
        self.assertTrue(math.isclose(2 ** 3, 8))


if __name__ == "__main__":
    unittest.main()
