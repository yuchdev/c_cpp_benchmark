#!/usr/bin/env python3

import argparse
import json
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

def generate_md(results: List[Dict[str, Any]]) -> str:
    if not results:
        return "No results to display."
    
    lines = []
    lines.append("# Benchmark Report")
    lines.append("")
    
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
            lines.append("| Benchmark | Language | Measure | Value | Unit | Samples |")
            lines.append("|:---|:---|:---|:---|:---|:---|")
            
            # Sort by benchmark, then language
            for r in sorted(group_results, key=lambda x: (x["benchmark"], x["language"])):
                val = r["value"]
                if isinstance(val, float):
                    val_str = f"{val:.6f}"
                else:
                    val_str = str(val)
                    
                lines.append(f"| {r['benchmark']} | {r['language']} | {r['measure']} | {val_str} | {r['unit']} | {r['samples']} |")
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
