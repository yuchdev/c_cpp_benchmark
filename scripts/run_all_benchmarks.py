#!/usr/bin/env python3
"""Run all generic + matrix benchmarks and export visualization-friendly results."""

from __future__ import annotations

import argparse
import csv
import sys
import json
import math
import statistics
import subprocess
import time
import re
from pathlib import Path
from typing import Dict, List, Optional


#: Generic benchmark definitions as ``(group, language, executable_name)`` tuples.
GENERIC_BENCHMARKS = [
    ("a", "c", "a_qsort_c"),
    ("a", "cpp", "a_std_sort_cpp"),
    ("b", "c", "b_callback_c"),
    ("b", "cpp", "b_template_cpp"),
    ("c", "c", "c_struct_api"),
    ("c", "cpp", "c_class_operator_cpp"),
    ("d", "c", "d_buffer_copy_c"),
    ("d", "cpp", "d_buffer_move_cpp"),
    ("e", "c", "e_runtime_table_c"),
    ("e", "cpp", "e_constexpr_table_cpp"),
]

#: Matrix benchmark definitions as ``(language, executable_name, raw_csv_filename)`` tuples.
MATRIX_BENCHMARKS = [
    ("c", "c_matrix_bench", "c_results.csv"),
    ("cpp", "cpp_matrix_bench", "cpp_results.csv"),
]


def find_executable(build_dir: Path, name: str) -> Path:
    """Locate an executable under the configured build directory.

    The lookup checks common subdirectories first, then performs a recursive
    wildcard search as a fallback.

    :param build_dir: Root build directory that contains benchmark artifacts.
    :param name: Executable base name to resolve.
    :returns: Absolute or relative path to the matching executable file.
    :raises FileNotFoundError: If no executable matching ``name`` is found.
    """
    candidates = [
        build_dir / "benchmarks" / "generic" / name,
        build_dir / "benchmarks" / "matrix" / name,
        build_dir / name,
    ]
    if build_dir.exists():
        for candidate in candidates:
            if candidate.exists() and candidate.is_file():
                return candidate
            if (candidate.with_suffix(".exe")).exists():
                return candidate.with_suffix(".exe")

        wildcard = f"{name}*"
        for path in build_dir.rglob(wildcard):
            if path.is_file() and path.stem == name:
                return path

    raise FileNotFoundError(f"Could not find executable '{name}' under {build_dir}")


def build_project(build_dir: Path) -> None:
    """Run CMake configure and build to generate missing executables.

    :param build_dir: Root build directory to build.
    :raises RuntimeError: If the build fails.
    """
    project_root = Path(__file__).parent.parent.resolve()
    
    if not (build_dir / "CMakeCache.txt").exists():
        print(f"Initializing build directory {build_dir}...")
        build_dir.mkdir(parents=True, exist_ok=True)
        # Using Release build type by default for benchmarks
        run_cmd(["cmake", "-S", str(project_root), "-B", str(build_dir), "-DCMAKE_BUILD_TYPE=Release"])

    print(f"Triggering build in {build_dir}...")
    # Using 'cmake --build <dir>' which is the standard cross-platform way
    run_cmd(["cmake", "--build", str(build_dir)])


def run_cmd(cmd: List[str]) -> None:
    """Run a command and raise if it fails.

    :param cmd: Command vector passed directly to :func:`subprocess.run`.
    :returns: ``None``.
    :raises RuntimeError: If the command exits with a non-zero status.
    """
    result = subprocess.run(cmd, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"Command failed with code {result.returncode}: {' '.join(cmd)}")


class TestingStrategy:
    """Represents a testing strategy as a sequence of sizes or a progression.
    
    Formats:
    - Sequence: "1000,10000,100000"
    - Progression: "start,multiplier,steps" e.g., "10000,x10,5"
    """
    def __init__(self, sizes: List[int]):
        self.sizes = sizes

    @classmethod
    def parse(cls, value: str) -> TestingStrategy:
        if not value:
            raise argparse.ArgumentTypeError("value must not be empty")
        
        # Check for progression format: start,xMult,steps
        if "x" in value and "," in value:
            parts = [p.strip() for p in value.split(",")]
            if len(parts) == 3 and parts[1].startswith("x"):
                try:
                    start = int(parts[0])
                    mult_str = parts[1][1:]
                    multiplier = float(mult_str) if "." in mult_str else int(mult_str)
                    steps = int(parts[2])
                    
                    sizes = []
                    current = start
                    for _ in range(steps):
                        sizes.append(int(current))
                        current *= multiplier
                    return cls(sizes)
                except ValueError as exc:
                    raise argparse.ArgumentTypeError(f"Invalid progression format: {value}") from exc

        # Fallback to sequence format
        try:
            sizes = []
            for item in value.split(","):
                item = item.strip()
                if not item: continue
                val = int(item)
                if val <= 0:
                    raise argparse.ArgumentTypeError(f"value must be positive: {val}")
                sizes.append(val)
            
            if not sizes:
                raise argparse.ArgumentTypeError("No valid sizes found")
            
            # Validate non-decreasing
            if any(sizes[i] > sizes[i+1] for i in range(len(sizes) - 1)):
                 raise argparse.ArgumentTypeError(f"values must be in non-decreasing order: {sizes}")
            
            return cls(sizes)
        except ValueError as exc:
            raise argparse.ArgumentTypeError(f"Invalid sequence format: {value}") from exc

    def __iter__(self):
        return iter(self.sizes)


def run_generic(build_dir: Path, repeats: int, rows: List[Dict[str, object]], sizing: Dict[str, TestingStrategy]) -> None:
    """Execute generic benchmark binaries and collect wall-clock timings.

    :param build_dir: Build directory where benchmark executables are located.
    :param repeats: Number of timing runs per benchmark executable.
    :param rows: Mutable row collection where run records are appended.
    :param sizing: Optional dictionary mapping group to TestingStrategy.
    :returns: ``None``.
    :raises FileNotFoundError: If a benchmark executable cannot be located.
    :raises RuntimeError: If a benchmark process is terminated by a signal.
    """
    did_build = False
    for group, lang, exe_name in GENERIC_BENCHMARKS:
        try:
            exe = find_executable(build_dir, exe_name)
        except FileNotFoundError:
            if not did_build:
                try:
                    build_project(build_dir)
                    did_build = True
                    exe = find_executable(build_dir, exe_name)
                except (RuntimeError, FileNotFoundError) as e:
                    raise FileNotFoundError(f"Could not find or build executable '{exe_name}': {e}") from e
            else:
                raise

        # Determine sizes to run
        strategy = sizing.get(group)
        sizes = list(strategy) if strategy else [None]
        
        for size in sizes:
            cmd = [str(exe)]
            if size is not None:
                cmd.append(str(size))
                
            for run_index in range(1, repeats + 1):
                result = subprocess.run(cmd, check=False, capture_output=True, text=True)
                if result.returncode < 0:
                    raise RuntimeError(f"{exe_name} terminated by signal {-result.returncode}: {result.stderr.strip()}")
                
                # Parse output for measure and time
                # Example: "cpp sort measure=1000000 time=0.045678 sec"
                output = result.stdout.strip()
                measure = size
                value = None
                
                # Match lines like "... measure=123 time=0.456 sec"
                match = re.search(r"measure=(\d+)\s+time=([\d.]+)", output)
                if match:
                    measure = int(match.group(1))
                    value = float(match.group(2))
                
                # Validation: ensure the benchmark actually used the requested size
                if size is not None and measure != size:
                    print(f"Warning: {exe_name} requested size {size} but reported measure {measure}", file=sys.stderr)
                
                # Fallback to group name if exe name starts with group_
                benchmark_name = exe_name
                
                row = {
                    "suite": "generic",
                    "group": group,
                    "benchmark": benchmark_name,
                    "language": lang,
                    "run": run_index,
                    "measure": measure,
                    "metric": "wall_time_sec",
                    "value": value if value is not None else 0.0,
                    "unit": "sec",
                }
                rows.append(row)


def _extract_matrix_op(name: str, rows_n: int) -> str:
    """Extract the pure operation name from a compound matrix benchmark name.

    Strips the language/type prefix (``c_``, ``cpp_dynamic_``, ``cpp_fixed_``)
    and the size suffix (``_NxN``).  For example::

        c_mul_128x128       (rows=128) → mul
        cpp_dynamic_matvec_512x512  (rows=512) → matvec
        cpp_fixed_transpose_mul_4x4 (rows=4)   → transpose_mul
    """
    suffix = f"_{rows_n}x{rows_n}"
    if name.endswith(suffix):
        name = name[: -len(suffix)]
    for prefix in ("cpp_dynamic_", "cpp_fixed_", "c_"):
        if name.startswith(prefix):
            return name[len(prefix):]
    return name


def run_matrix(build_dir: Path, output_dir: Path, rows: List[Dict[str, object]]) -> None:
    """Execute matrix benchmarks and ingest their generated CSV outputs.

    :param build_dir: Build directory where matrix benchmark executables exist.
    :param output_dir: Parent output directory for raw and summarized artifacts.
    :param rows: Mutable row collection where parsed benchmark data is appended.
    :returns: ``None``.
    :raises FileNotFoundError: If a matrix benchmark executable is not found.
    :raises RuntimeError: If a matrix benchmark process fails.
    :raises KeyError: If expected CSV columns are missing from benchmark output.
    :raises ValueError: If ``avg_ns`` cannot be converted to ``float``.
    """
    raw_dir = output_dir / "matrix_raw"
    raw_dir.mkdir(parents=True, exist_ok=True)

    did_build = False
    for lang, exe_name, csv_name in MATRIX_BENCHMARKS:
        try:
            exe = find_executable(build_dir, exe_name)
        except FileNotFoundError:
            if not did_build:
                try:
                    build_project(build_dir)
                    did_build = True
                    exe = find_executable(build_dir, exe_name)
                except (RuntimeError, FileNotFoundError) as e:
                    raise FileNotFoundError(f"Could not find or build executable '{exe_name}': {e}") from e
            else:
                raise

        out_csv = raw_dir / csv_name
        run_cmd([str(exe), str(out_csv)])

        with out_csv.open(newline="", encoding="utf-8") as f:
            reader = csv.DictReader(f)
            for row in reader:
                size = int(row["rows"])
                rows.append(
                    {
                        "suite": "matrix",
                        "group": "matrix",
                        "benchmark": _extract_matrix_op(row["name"], size),
                        "language": lang,
                        "run": 1,
                        "measure": size,
                        "metric": "avg_ns",
                        "value": float(row["avg_ns"]),
                        "unit": "ns",
                    }
                )


def write_outputs(rows: List[Dict[str, object]], output_dir: Path) -> None:
    """Write benchmark run rows and aggregate summaries to disk.

    :param rows: Flat list of benchmark sample rows.
    :param output_dir: Destination directory for results.
    :returns: ``None``.
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    runs_csv = output_dir / "runs.csv"
    runs_json = output_dir / "runs.json"

    fieldnames = ["suite", "group", "benchmark", "language", "run", "measure", "metric", "value", "unit"]
    
    # Save all data to runs.csv
    with runs_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    # Separate CSV for each type (group)
    groups = set(row["group"] for row in rows)
    for group in groups:
        group_rows = [row for row in rows if row["group"] == group]
        group_csv = output_dir / f"results_{group}.csv"
        with group_csv.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(group_rows)

    # Prepare data for runs.json (averaging repeats)
    grouped_for_json: Dict[tuple, List[float]] = {}
    for row in rows:
        # Key: (suite, group, benchmark, language, measure, metric, unit)
        key = (row["suite"], row["group"], row["benchmark"], row["language"], row["measure"], row["metric"], row["unit"])
        grouped_for_json.setdefault(key, []).append(float(row["value"]))

    json_rows = []
    for key, values in grouped_for_json.items():
        json_rows.append({
            "suite": key[0],
            "group": key[1],
            "benchmark": key[2],
            "language": key[3],
            "measure": key[4],
            "metric": key[5],
            "value": sum(values) / len(values),
            "unit": key[6],
            "samples": len(values)
        })

    # Save averaged data to runs.json
    with runs_json.open("w", encoding="utf-8") as f:
        json.dump(json_rows, f, indent=2)

    # Generate summary
    summary_csv = output_dir / "summary.csv"
    grouped: Dict[tuple, List[float]] = {}
    for row in rows:
        key = (row["suite"], row["group"], row["benchmark"], row["language"], row["measure"], row["metric"], row["unit"])
        grouped.setdefault(key, []).append(float(row["value"]))

    summary_fields = ["suite", "group", "benchmark", "language", "measure", "metric", "unit", "samples", "mean", "min", "max", "stdev"]
    with summary_csv.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=summary_fields)
        writer.writeheader()
        
        def sort_key(k):
            # (suite, group, benchmark, language, measure, metric, unit)
            measure = k[4]
            if measure is None:
                measure = 0
            return (k[0], k[1], k[2], k[3], measure)

        for key in sorted(grouped.keys(), key=sort_key):
            values = grouped[key]
            stdev = statistics.stdev(values) if len(values) > 1 else 0.0
            writer.writerow(
                {
                    "suite": key[0],
                    "group": key[1],
                    "benchmark": key[2],
                    "language": key[3],
                    "measure": key[4],
                    "metric": key[5],
                    "unit": key[6],
                    "samples": len(values),
                    "mean": statistics.mean(values),
                    "min": min(values),
                    "max": max(values),
                    "stdev": stdev if not math.isnan(stdev) else 0.0,
                }
            )


def plot_from_dir(results_dir: Path, plots_dir: Path, *, chart: str = "line",
                  article_mode: bool = False) -> None:
    """Generate plots from a completed benchmark-results directory.

    The function prefers ``runs.csv`` and falls back to ``runs.json``.

    :param results_dir: Directory containing ``runs.csv``/``runs.json``.
    :param plots_dir: Destination directory for the generated plots.
    :param chart: Per-operation chart style (``"line"`` or ``"bar"``).
    :param article_mode: Enable Dev.to-optimized styling.
    :raises FileNotFoundError: If no recognizable results file is found.
    """
    import plot_results

    source = None
    for candidate in ("runs.csv", "runs.json"):
        path = results_dir / candidate
        if path.exists():
            source = path
            break
    if source is None:
        raise FileNotFoundError(
            f"No runs.csv or runs.json found in {results_dir}"
        )

    results = plot_results.load_results(source)
    if not results:
        print(f"Warning: no results parsed from {source}", file=sys.stderr)
        return
    written = plot_results.generate_all(
        results, plots_dir, chart=chart, article_mode=article_mode
    )
    print(f"Generated {len(written)} plot artifacts in {plots_dir}")


def main() -> int:
    """Parse CLI arguments, run selected benchmark suites, and export results.

    :returns: Process exit code ``0`` on success.
    :raises ValueError: If ``--repeats`` is less than ``1``.
    :raises FileNotFoundError: If required benchmark executables are missing.
    :raises RuntimeError: If a benchmark subprocess fails.
    :raises OSError: If output artifacts cannot be written.
    """
    parser = argparse.ArgumentParser(description="Run all repository benchmarks and export tabular results")
    parser.add_argument("--build-dir", default="build", help="CMake build directory")
    parser.add_argument("--output-dir", default="benchmark_results", help="Output directory for CSV/JSON artifacts")
    parser.add_argument("--repeats", type=int, default=5, help="Repeat count for generic executable timing")
    parser.add_argument("--skip-generic", action="store_true", help="Skip generic benchmarks")
    parser.add_argument("--skip-matrix", action="store_true", help="Skip matrix benchmarks")

    parser.add_argument("--sort", type=TestingStrategy.parse, help="Vector sizes for sort benchmarks (group a)")
    parser.add_argument("--callback", type=TestingStrategy.parse, help="Iteration counts for callback benchmarks (group b)")
    parser.add_argument("--struct-api", type=TestingStrategy.parse, help="Iteration counts for struct API benchmarks (group c)")
    parser.add_argument("--buffer", type=TestingStrategy.parse, help="Buffer sizes for copy/move benchmarks (group d)")
    parser.add_argument("--table", type=TestingStrategy.parse, help="Iteration counts for table benchmarks (group e)")

    # Visualization options.
    parser.add_argument("--plot", action="store_true",
                        help="Generate Matplotlib plots after the run completes")
    parser.add_argument("--plot-only", metavar="DIR",
                        help="Skip running; plot an existing benchmark-results directory and exit")
    parser.add_argument("--plots-dir",
                        help="Output directory for plots (default: <output-dir>/plots)")
    parser.add_argument("--chart", choices=["line", "bar"], default="line",
                        help="Per-operation chart style (default: line)")
    parser.add_argument("--article-mode", action="store_true",
                        help="Render 840x420 article-optimized plots")

    args = parser.parse_args()

    # Plot-only mode: visualize existing results without running benchmarks.
    if args.plot_only:
        results_dir = Path(args.plot_only).resolve()
        plots_dir = Path(args.plots_dir).resolve() if args.plots_dir else results_dir / "plots"
        plot_from_dir(results_dir, plots_dir, chart=args.chart,
                      article_mode=args.article_mode)
        return 0

    if args.repeats < 1:
        raise ValueError("--repeats must be >= 1")

    build_dir = Path(args.build_dir).resolve()
    output_dir = Path(args.output_dir).resolve()

    if output_dir.exists() and any(output_dir.iterdir()):
        import shutil
        print(f"Cleaning output directory: {output_dir}")
        for item in output_dir.iterdir():
            if item.is_dir():
                shutil.rmtree(item)
            else:
                item.unlink()

    sizing = {}
    if args.sort: sizing["a"] = args.sort
    if args.callback: sizing["b"] = args.callback
    if args.struct_api: sizing["c"] = args.struct_api
    if args.buffer: sizing["d"] = args.buffer
    if args.table: sizing["e"] = args.table

    rows: List[Dict[str, object]] = []
    if not args.skip_generic:
        run_generic(build_dir, args.repeats, rows, sizing)
    if not args.skip_matrix:
        run_matrix(build_dir, output_dir, rows)
    write_outputs(rows, output_dir)

    print(f"Wrote {len(rows)} rows to {output_dir / 'runs.csv'}")
    print(f"Summary: {output_dir / 'summary.csv'}")
    print(f"JSON: {output_dir / 'runs.json'}")

    if args.plot:
        plots_dir = Path(args.plots_dir).resolve() if args.plots_dir else output_dir / "plots"
        plot_from_dir(output_dir, plots_dir, chart=args.chart,
                      article_mode=args.article_mode)

    return 0


if __name__ == "__main__":
    sys.exit(main())
