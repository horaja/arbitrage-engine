#!/usr/bin/env python3
"""Normalize Google Benchmark JSON output into canonical format.

Usage:
  ./build/my_gbench --benchmark_format=json --benchmark_out=gbench.json

  python3 tools/normalizers/normalize_google_benchmark.py gbench.json
"""

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

from common import default_output_path, make_envelope, metric, write_result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert Google Benchmark JSON to canonical benchmark format."
    )
    parser.add_argument("input", type=Path, help="Path to Google Benchmark JSON output.")
    parser.add_argument("--output", type=Path, help="Output path (default: auto-generated).")
    parser.add_argument("--tags", nargs="*", default=[], help="Optional tags.")
    return parser.parse_args()


def normalize(report: dict) -> tuple[dict, dict[str, dict]]:
    """Extract config and metrics from Google Benchmark JSON."""
    context = report.get("context", {})
    config = {
        "num_cpus": context.get("num_cpus", 0),
        "mhz_per_cpu": context.get("mhz_per_cpu", 0),
        "library_build_type": context.get("library_build_type", ""),
    }

    metrics: dict[str, dict] = {}

    for bench in report.get("benchmarks", []):
        name = bench.get("name", "unknown")
        # Sanitize benchmark name for use as metric prefix
        prefix = name.replace("/", "_").replace(" ", "_").lower()

        time_unit = bench.get("time_unit", "ns")

        if "real_time" in bench:
            metrics[f"{prefix}_real_time"] = metric(bench["real_time"], time_unit)
        if "cpu_time" in bench:
            metrics[f"{prefix}_cpu_time"] = metric(bench["cpu_time"], time_unit)
        if "iterations" in bench:
            metrics[f"{prefix}_iterations"] = metric(bench["iterations"], "count")
        if "bytes_per_second" in bench:
            metrics[f"{prefix}_bytes_per_second"] = metric(bench["bytes_per_second"], "bytes/s")
        if "items_per_second" in bench:
            metrics[f"{prefix}_items_per_second"] = metric(bench["items_per_second"], "items/s")

        # Capture any user-defined counters
        for key, value in bench.items():
            if key.startswith("counter_") or key not in {
                "name", "family_index", "per_family_instance_index",
                "run_name", "run_type", "repetitions", "repetition_index",
                "threads", "iterations", "real_time", "cpu_time",
                "time_unit", "bytes_per_second", "items_per_second",
                "aggregate_name", "aggregate_unit",
            }:
                if isinstance(value, (int, float)):
                    metrics[f"{prefix}_{key}"] = metric(value, "count")

    return config, metrics


def main() -> int:
    args = parse_args()
    try:
        with args.input.open() as f:
            report = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"error reading input: {e}")
        return 1

    config, metrics = normalize(report)
    if not metrics:
        print("error: no benchmarks found in input")
        return 1

    now = datetime.now(timezone.utc)
    result = make_envelope("google_benchmark", config, metrics, tags=args.tags, timestamp=now)
    output_path = args.output or default_output_path("google_benchmark", now)
    write_result(result, output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
