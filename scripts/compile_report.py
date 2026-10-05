#!/usr/bin/env python3

import argparse
import json
import math
import sys
from pathlib import Path
from typing import List, Dict, Any, Optional


def load_results(input_dir: Path) -> List[Dict[str, Any]]:
    runs_json = input_dir / "runs.json"
    if not runs_json.exists():
        print(f"Error: {runs_json} not found.")
        sys.exit(1)
    
    with runs_json.open("r", encoding="utf-8") as f:
        return json.load(f)


def filter_results(
    results: List[Dict[str, Any]], 
    suites: Optional[List[str]] = None, 
    groups: Optional[List[str]] = None, 
    benchmarks: Optional[List[str]] = None
) -> List[Dict[str, Any]]:
    filtered = results
    if suites:
        filtered = [r for r in filtered if r["suite"] in suites]
    if groups:
        filtered = [r for r in filtered if r["group"] in groups]
    if benchmarks:
        filtered = [r for r in filtered if r["benchmark"] in benchmarks]
    return filtered


def _speedup_by_cpp_row(results: List[Dict[str, Any]]) -> Dict[int, str]:
    """Calculate paired C/C++ speedups, indexed by each C++ result row."""
    candidates: Dict[Any, Dict[str, List[Dict[str, Any]]]] = {}
    for result in results:
        language = str(result.get("language", "")).strip().lower()
        if language in ("c", "cpp", "c++", "cxx", "cplusplus"):
            key = (
                result.get("suite"),
                result.get("group"),
                result.get("measure"),
                result.get("metric"),
            )
            language_key = "cpp" if language != "c" else "c"
            candidates.setdefault(key, {}).setdefault(language_key, []).append(result)

    speedups: Dict[int, str] = {}
    for result in results:
        language = str(result.get("language", "")).strip().lower()
        if language not in ("cpp", "c++", "cxx", "cplusplus"):
            continue
        key = (
            result.get("suite"),
            result.get("group"),
            result.get("measure"),
            result.get("metric"),
        )
        group_candidates = candidates.get(key, {})
        c_candidates = group_candidates.get("c", [])
        cpp_candidates = group_candidates.get("cpp", [])
        matching_benchmark = [
            candidate for candidate in c_candidates
            if candidate.get("benchmark") == result.get("benchmark")
        ]
        if len(matching_benchmark) == 1:
            c_result = matching_benchmark[0]
        elif len(c_candidates) == 1 and len(cpp_candidates) == 1:
            c_result = c_candidates[0]
        else:
            continue

        c_time = float(c_result["value"])
        cpp_time = float(result["value"])
        if not math.isfinite(c_time) or not math.isfinite(cpp_time) or c_time <= 0 or cpp_time <= 0:
            speedups[id(result)] = "N/A"
            continue

        ratio = c_time / cpp_time
        if ratio > 1:
            speedups[id(result)] = f"{ratio:.2f}× faster"
        elif ratio < 1:
            speedups[id(result)] = f"{1 / ratio:.2f}× slower"
        else:
            speedups[id(result)] = "Same speed"
    return speedups


def generate_md(results: List[Dict[str, Any]]) -> str:
    if not results:
        return "No results to display."
    
    lines = []
    lines.append("# Benchmark Report")
    lines.append("")
    lines.append(
        "C++ speedup is C time / C++ time, paired by benchmark name or by a unique C/C++ pair "
        "within the same suite, group, measure, and metric."
    )
    lines.append("")
    speedups = _speedup_by_cpp_row(results)
    
    # Group by suite and group
    suites = sorted(set(r["suite"] for r in results))
    for suite in suites:
        lines.append(f"## Suite: {suite}")
        lines.append("")
        
        suite_results = [r for r in results if r["suite"] == suite]
        groups = sorted(set(r["group"] for r in suite_results))
        
        for group in groups:
            lines.append(f"### Group: {group}")
            lines.append("")
            
            group_results = [r for r in suite_results if r["group"] == group]
            
            # Header
            lines.append("| Benchmark | Language | Measure | Value | Unit | Samples | C++ vs C |")
            lines.append("|:---|:---|:---|:---|:---|:---|:---|")
            
            # Sort by benchmark, then language
            for r in sorted(group_results, key=lambda x: (x["benchmark"], x["language"])):
                val = r["value"]
                if isinstance(val, float):
                    val_str = f"{val:.6f}"
                else:
                    val_str = str(val)
                    
                speedup = speedups.get(id(r), "")
                lines.append(
                    f"| {r['benchmark']} | {r['language']} | {r['measure']} | {val_str} | "
                    f"{r['unit']} | {r['samples']} | {speedup} |"
                )
            lines.append("")
            
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Compile benchmark report from runs.json")
    parser.add_argument("input_dir", type=Path, help="Directory containing runs.json")
    parser.add_argument("--output-format", choices=["md", "html", "pdf"], default="md", help="Output format (default: md)")
    parser.add_argument("--output-file", type=Path, help="File to write the report to (default: stdout)")
    
    parser.add_argument("--suites", help="Comma-separated list of suites to include")
    parser.add_argument("--groups", help="Comma-separated list of groups to include")
    parser.add_argument("--benchmarks", help="Comma-separated list of benchmarks to include")
    
    args = parser.parse_args()
    
    try:
        if args.output_format != "md":
            raise NotImplementedError(f"Output format '{args.output_format}' is not implemented yet.")
        
        results = load_results(args.input_dir)
        
        suites_filter = args.suites.split(",") if args.suites else None
        groups_filter = args.groups.split(",") if args.groups else None
        benchmarks_filter = args.benchmarks.split(",") if args.benchmarks else None
        
        filtered_results = filter_results(results, suites_filter, groups_filter, benchmarks_filter)
        
        report = generate_md(filtered_results)
        
        if args.output_file:
            args.output_file.write_text(report, encoding="utf-8")
            print(f"Report written to {args.output_file}")
        else:
            print(report)
            
    except NotImplementedError as e:
        print(f"Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
