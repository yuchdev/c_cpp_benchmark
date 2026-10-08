#!/usr/bin/env python3
"""Unit tests for the matrix-ingestion half of ``scripts/run_all_benchmarks.py``.

No real benchmark is built or run: ``run_matrix`` is exercised against tiny fake
``c_matrix_bench`` / ``cpp_matrix_bench`` executables that record their command
line and write a CSV in the exact schema the real binaries produce.  That pins
down

* how a CSV row name is classified into ``(group, benchmark)`` (the contract with
  ``benchmarks/matrix/benchmarks/bench_scenarios.h``),
* which command-line options are forwarded to the binaries, and
* the rows, groups and per-group result files that come out the other side.
"""

import csv
import json
import os
import stat
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import mock

_SCRIPTS = Path(__file__).resolve().parent.parent / "scripts"
sys.path.insert(0, str(_SCRIPTS))

import run_all_benchmarks as rab  # noqa: E402


class TestClassifyMatrixRow(unittest.TestCase):
    """``classify_matrix_row`` strips prefix/suffix and routes scenarios to their group."""

    CASES = [
        # core grid: C and C++ dynamic rows share group "matrix" and the bare op name
        ("c_mul_128x128", ("matrix", "mul")),
        ("c_transpose_mul_8x8", ("matrix", "transpose_mul")),
        ("c_add3_4x4", ("matrix", "add3")),
        ("cpp_dynamic_matvec_512x512", ("matrix", "matvec")),
        ("cpp_dynamic_mul_add_32x32", ("matrix", "mul_add")),
        # scenarios: own group, scenario-specific variant (with optional _n<param>)
        ("c_chain_add_n256_8x256", ("matrix_chain", "add_n256")),
        ("cpp_chain_add_n32_16x32", ("matrix_chain", "add_n32")),
        ("c_fixed_mul_4x4", ("matrix_fixed", "mul")),
        ("cpp_fixed_transpose_mul_16x16", ("matrix_fixed", "transpose_mul")),
        ("c_cliff_matvec_2048x2048", ("matrix_cliff", "matvec")),
        ("cpp_batch_xform4_1048576x4", ("matrix_batch", "xform4")),
        ("c_block_copy_n512_4x4", ("matrix_block", "copy_n512")),
        ("cpp_block_mul_n512_256x256", ("matrix_block", "mul_n512")),
        ("c_tri_trsm_32x32", ("matrix_tri", "trsm")),
        ("cpp_tri_syrk_256x256", ("matrix_tri", "syrk")),
        ("c_conv_blur3x3_64x64", ("matrix_conv", "blur3x3")),
    ]

    def test_cases(self):
        for name, expected in self.CASES:
            with self.subTest(name=name):
                self.assertEqual(rab.classify_matrix_row(name), expected)

    def test_c_and_cpp_rows_of_one_benchmark_pair_up(self):
        """The Python side pairs languages by (group, benchmark): they must agree."""
        for c_name, cpp_name in [
            ("c_chain_add_n64_4x64", "cpp_chain_add_n64_4x64"),
            ("c_fixed_add3_7x7", "cpp_fixed_add3_7x7"),
            ("c_mul_64x64", "cpp_dynamic_mul_64x64"),
            ("c_block_mul_n512_32x32", "cpp_block_mul_n512_32x32"),
        ]:
            with self.subTest(c=c_name):
                self.assertEqual(rab.classify_matrix_row(c_name), rab.classify_matrix_row(cpp_name))

    def test_core_op_is_never_mistaken_for_a_scenario(self):
        for op in ("transpose", "add", "sub", "scale", "matvec", "mul",
                   "transpose_mul", "add3", "mul_add"):
            self.assertEqual(rab.classify_matrix_row(f"c_{op}_16x16"), ("matrix", op))

    def test_every_documented_scenario_is_known(self):
        self.assertEqual(
            set(rab.MATRIX_SCENARIOS),
            {"chain", "fixed", "cliff", "batch", "block", "tri", "conv"},
        )


_FAKE_EXE = """#!{python} -S
import json, sys
argv = sys.argv[1:]
csv_path = argv[argv.index("--csv") + 1]
with open(csv_path, "w") as f:
    f.write("name,rows,cols,iterations,avg_ns\\n")
    for line in {rows!r}:
        f.write(line + "\\n")
with open({record!r}, "w") as f:
    json.dump(argv, f)
"""

#: (name, rows, cols, iters, avg_ns) per language; every benchmark has both languages.
_ROWS = {
    "c": [
        "c_mul_32x32,32,32,20,5000.0",
        "c_chain_add_n64_4x64,4,64,20,900.0",
        "c_fixed_mul_4x4,4,4,1000,40.0",
        "c_batch_xform4_1024x4,1024,4,320,3200.0",
        "c_conv_blur3x3_64x64,64,64,142,6852.0",
    ],
    "cpp": [
        "cpp_dynamic_mul_32x32,32,32,20,1000.0",
        "cpp_chain_add_n64_4x64,4,64,20,300.0",
        "cpp_fixed_mul_4x4,4,4,1000,4.0",
        "cpp_batch_xform4_1024x4,1024,4,320,1700.0",
        "cpp_conv_blur3x3_64x64,64,64,142,10349.0",
    ],
}


def _make_fake_build(root: Path):
    """Create <root>/benchmarks/matrix/{c,cpp}_matrix_bench; returns the argv record paths."""
    exe_dir = root / "benchmarks" / "matrix"
    exe_dir.mkdir(parents=True)
    records = {}
    for lang, exe in (("c", "c_matrix_bench"), ("cpp", "cpp_matrix_bench")):
        record = root / f"{lang}_argv.json"
        records[lang] = record
        path = exe_dir / exe
        path.write_text(
            _FAKE_EXE.format(python=sys.executable, rows=_ROWS[lang], record=str(record)),
            encoding="utf-8",
        )
        path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return records


@unittest.skipIf(os.name == "nt", "fake executables rely on a shebang line")
class TestRunMatrixIngestion(unittest.TestCase):
    """``run_matrix`` against fake binaries (created once: a fresh executable is slow to
    launch the first time on macOS, so sharing them keeps the suite fast)."""

    @classmethod
    def setUpClass(cls):
        cls._tmp = TemporaryDirectory()
        cls._root = Path(cls._tmp.name)
        cls._records = _make_fake_build(cls._root / "build")

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def _run(self, **kwargs):
        tmp = TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        out = Path(tmp.name)
        rows = []
        rab.run_matrix(self._root / "build", out, rows, **kwargs)
        argv = {lang: json.loads(p.read_text()) for lang, p in self._records.items()}
        return rows, out, argv

    def test_rows_are_grouped_and_paired(self):
        rows, _, _ = self._run()
        self.assertEqual(len(rows), 10)
        by_key = {(r["group"], r["benchmark"], r["language"], r["measure"]): r for r in rows}
        # core
        self.assertEqual(by_key[("matrix", "mul", "c", 32)]["value"], 5000.0)
        self.assertEqual(by_key[("matrix", "mul", "cpp", 32)]["value"], 1000.0)
        # the sweep parameter is the CSV `rows` column (chain depth k, M points, ...)
        self.assertEqual(by_key[("matrix_chain", "add_n64", "c", 4)]["value"], 900.0)
        self.assertEqual(by_key[("matrix_batch", "xform4", "cpp", 1024)]["value"], 1700.0)
        self.assertIn(("matrix_fixed", "mul", "cpp", 4), by_key)
        self.assertIn(("matrix_conv", "blur3x3", "c", 64), by_key)

    def test_every_row_has_the_schema_the_plotter_reads(self):
        rows, _, _ = self._run()
        for r in rows:
            self.assertEqual(r["suite"], "matrix")
            self.assertEqual(r["metric"], "avg_ns")
            self.assertEqual(r["unit"], "ns")
            self.assertIn(r["language"], ("c", "cpp"))

    def test_image_dir_is_always_passed_and_created(self):
        _, out, argv = self._run()
        for lang in ("c", "cpp"):
            self.assertIn("--image-dir", argv[lang])
            image_dir = Path(argv[lang][argv[lang].index("--image-dir") + 1])
            self.assertEqual(image_dir, out / "matrix_raw" / "images")
            self.assertTrue(image_dir.is_dir())

    def test_defaults_forward_no_optional_flags(self):
        _, _, argv = self._run()
        for lang in ("c", "cpp"):
            for flag in ("--scenarios", "--sizes", "--ops", "--repeats"):
                self.assertNotIn(flag, argv[lang])

    def test_options_are_forwarded_to_both_binaries(self):
        _, _, argv = self._run(sizes="4,8", ops="mul,add", repeats=3,
                               scenarios="chain,fixed")
        for lang in ("c", "cpp"):
            a = argv[lang]
            self.assertEqual(a[a.index("--scenarios") + 1], "chain,fixed")
            self.assertEqual(a[a.index("--sizes") + 1], "4,8")
            self.assertEqual(a[a.index("--ops") + 1], "mul,add")
            self.assertEqual(a[a.index("--repeats") + 1], "3")

    def test_raw_csvs_are_kept(self):
        _, out, _ = self._run()
        self.assertTrue((out / "matrix_raw" / "c_results.csv").exists())
        self.assertTrue((out / "matrix_raw" / "cpp_results.csv").exists())

    def test_missing_executable_attempts_a_build_then_raises(self):
        with TemporaryDirectory() as d, \
                mock.patch.object(rab, "build_project",
                                  side_effect=RuntimeError("no toolchain")) as build:
            with self.assertRaises(FileNotFoundError):
                rab.run_matrix(Path(d), Path(d) / "out", [])
            build.assert_called_once()


class TestWriteOutputsPerScenario(unittest.TestCase):
    """Each scenario group gets its own ``results_<group>.csv`` and shows up in the summary."""

    def test_group_files_and_summary(self):
        rows = []
        for group, bench in (("matrix", "mul"), ("matrix_chain", "add_n64"),
                             ("matrix_fixed", "mul")):
            for lang, value in (("c", 100.0), ("cpp", 25.0)):
                rows.append({"suite": "matrix", "group": group, "benchmark": bench,
                             "language": lang, "run": 1, "measure": 4,
                             "metric": "avg_ns", "value": value, "unit": "ns"})
        with TemporaryDirectory() as d:
            out = Path(d)
            rab.write_outputs(rows, out)
            for group in ("matrix", "matrix_chain", "matrix_fixed"):
                self.assertTrue((out / f"results_{group}.csv").exists(), group)
            with (out / "summary.csv").open(newline="", encoding="utf-8") as f:
                groups = {r["group"] for r in csv.DictReader(f)}
            self.assertEqual(groups, {"matrix", "matrix_chain", "matrix_fixed"})
            data = json.loads((out / "runs.json").read_text(encoding="utf-8"))
            self.assertEqual(len(data), 6)  # nothing merged across groups


if __name__ == "__main__":
    unittest.main()
