#!/usr/bin/env python3
"""Compare canonical benchmark results across commits and tools.

Usage:
  # Compare all results for a metric:
  python3 tools/compare_benchmarks.py --metric throughput_events_per_second_mean

  # Filter by tool:
  python3 tools/compare_benchmarks.py --metric throughput_events_per_second_mean --tool arb_benchmark

  # Filter by tag:
  python3 tools/compare_benchmarks.py --metric logic_latency_p99_ns --tag baseline

  # Compare two specific commits:
  python3 tools/compare_benchmarks.py --metric cycles --commits 1fc34fb,3b54b86

  # List all available metrics across results:
  python3 tools/compare_benchmarks.py --list-metrics

  # List all results:
  python3 tools/compare_benchmarks.py --list
"""

import argparse
import json
from pathlib import Path


RESULTS_DIR = Path("benchmarks/results")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare canonical benchmark results.")
    parser.add_argument("--results-dir", type=Path, default=RESULTS_DIR)
    parser.add_argument("--metric", type=str, help="Metric name to compare.")
    parser.add_argument("--tool", type=str, help="Filter by tool name.")
    parser.add_argument("--tag", type=str, help="Filter by tag.")
    parser.add_argument("--commits", type=str, help="Comma-separated commit hashes to compare.")
    parser.add_argument("--list-metrics", action="store_true", help="List all metrics found.")
    parser.add_argument("--list", action="store_true", help="List all result files.")
    return parser.parse_args()


def load_results(results_dir: Path) -> list[dict]:
    results = []
    if not results_dir.exists():
        return results
    for path in sorted(results_dir.glob("*.json")):
        try:
            with path.open() as f:
                data = json.load(f)
            data["_path"] = str(path)
            data["_filename"] = path.name
            results.append(data)
        except (json.JSONDecodeError, OSError):
            continue
    return results


def filter_results(
    results: list[dict],
    tool: str | None = None,
    tag: str | None = None,
    commits: list[str] | None = None,
) -> list[dict]:
    filtered = results
    if tool:
        filtered = [r for r in filtered if r.get("tool") == tool]
    if tag:
        filtered = [r for r in filtered if tag in r.get("tags", [])]
    if commits:
        filtered = [r for r in filtered if r.get("commit") in commits]
    return filtered


def list_all_metrics(results: list[dict]) -> None:
    metrics_by_tool: dict[str, set[str]] = {}
    for r in results:
        tool = r.get("tool", "unknown")
        for name in r.get("metrics", {}):
            metrics_by_tool.setdefault(tool, set()).add(name)

    for tool in sorted(metrics_by_tool):
        print(f"\n{tool}:")
        for name in sorted(metrics_by_tool[tool]):
            print(f"  {name}")


def list_results(results: list[dict]) -> None:
    print(f"{'filename':<55} {'tool':<20} {'commit':<10} {'tags'}")
    print("-" * 100)
    for r in results:
        tags = ", ".join(r.get("tags", []))
        print(f"{r['_filename']:<55} {r.get('tool', '?'):<20} {r.get('commit', '?'):<10} {tags}")


def compare_metric(results: list[dict], metric_name: str) -> None:
    rows: list[tuple[str, str, str, float, str]] = []
    for r in results:
        m = r.get("metrics", {}).get(metric_name)
        if m is None:
            continue
        rows.append((
            r.get("commit", "?"),
            r.get("tool", "?"),
            r.get("timestamp", "?")[:19],
            m["value"],
            m.get("unit", ""),
        ))

    if not rows:
        print(f"no results found for metric '{metric_name}'")
        return

    rows.sort(key=lambda x: x[2])  # Sort by timestamp

    print(f"\nMetric: {metric_name}")
    print(f"{'commit':<10} {'tool':<20} {'timestamp':<20} {'value':>15} {'unit':<10}")
    print("-" * 80)

    prev_value = None
    for commit, tool, ts, value, unit in rows:
        delta = ""
        if prev_value is not None and prev_value != 0:
            pct = (value - prev_value) / abs(prev_value) * 100
            sign = "+" if pct >= 0 else ""
            delta = f"  ({sign}{pct:.1f}%)"
        print(f"{commit:<10} {tool:<20} {ts:<20} {value:>15.2f} {unit:<10}{delta}")
        prev_value = value


def main() -> int:
    args = parse_args()
    results = load_results(args.results_dir)

    if not results:
        print(f"no results found in {args.results_dir}")
        return 1

    if args.list:
        list_results(results)
        return 0

    if args.list_metrics:
        list_all_metrics(results)
        return 0

    commits = args.commits.split(",") if args.commits else None
    filtered = filter_results(results, tool=args.tool, tag=args.tag, commits=commits)

    if args.metric:
        compare_metric(filtered, args.metric)
    else:
        print("specify --metric, --list-metrics, or --list")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
