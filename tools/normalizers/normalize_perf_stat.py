#!/usr/bin/env python3
"""Normalize `perf stat` output into canonical format.

Usage:
  perf stat -e cycles,instructions,cache-misses,cache-references,branch-misses,task-clock \
    ./build/arb_benchmark --input fixtures/sample_replay.csv 2> perf_output.txt

  python3 tools/normalizers/normalize_perf_stat.py perf_output.txt
"""

import argparse
import re
from datetime import datetime, timezone
from pathlib import Path

from common import default_output_path, make_envelope, metric, write_result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert perf stat text output to canonical benchmark format."
    )
    parser.add_argument("input", type=Path, help="Path to perf stat stderr output.")
    parser.add_argument("--command", type=str, default="", help="The command that was profiled.")
    parser.add_argument("--output", type=Path, help="Output path (default: auto-generated).")
    parser.add_argument("--tags", nargs="*", default=[], help="Optional tags.")
    return parser.parse_args()


# perf stat lines look like:
#   1,234,567      cycles                    #    3.456 GHz
#        0.45      task-clock (msec)         #    0.987 CPUs utilized
# or with no comment:
#   5,678,901      instructions
PERF_LINE_RE = re.compile(
    r"^\s*"
    r"(?P<value>[\d,]+(?:\.\d+)?)"
    r"\s+"
    r"(?P<event>[\w\-:]+)"
    r"(?:\s+.*)?$"
)

# Derived metric lines like:
#   1.42  insn per cycle
IPC_RE = re.compile(r"^\s*(?P<value>[\d.]+)\s+insn per cycle")

# Elapsed time line:
#   0.123456789 seconds time elapsed
ELAPSED_RE = re.compile(r"^\s*(?P<value>[\d.]+)\s+seconds time elapsed")


UNIT_MAP: dict[str, str] = {
    "cycles": "count",
    "instructions": "count",
    "cache-misses": "count",
    "cache-references": "count",
    "L1-dcache-loads": "refs",
    "L1-dcache-load-misses": "count",
    "L1-icache-load-misses": "count",
    "LLC-loads": "count",
    "LLC-load-misses": "count",
    "branch-misses": "count",
    "branch-instructions": "count",
    "task-clock": "ms",
    "context-switches": "count",
    "cpu-migrations": "count",
    "page-faults": "count",
}


def parse_perf_output(text: str) -> dict[str, dict]:
    metrics: dict[str, dict] = {}

    for line in text.splitlines():
        m = PERF_LINE_RE.match(line)
        if m:
            raw_value = m.group("value").replace(",", "")
            event = m.group("event")
            try:
                value = float(raw_value) if "." in raw_value else int(raw_value)
            except ValueError:
                continue
            unit = UNIT_MAP.get(event, "count")
            metric_name = event.replace("-", "_")
            metrics[metric_name] = metric(value, unit)
            continue

        m = IPC_RE.match(line)
        if m:
            metrics["ipc"] = metric(float(m.group("value")), "ratio")
            continue

        m = ELAPSED_RE.match(line)
        if m:
            metrics["elapsed_seconds"] = metric(float(m.group("value")), "s")

    # Derived: cache miss rate
    if "cache_misses" in metrics and "cache_references" in metrics:
        refs = metrics["cache_references"]["value"]
        if refs > 0:
            rate = metrics["cache_misses"]["value"] / refs * 100.0
            metrics["cache_miss_rate"] = metric(round(rate, 4), "percent")

    # Derived: branch miss rate
    if "branch_misses" in metrics and "branch_instructions" in metrics:
        total = metrics["branch_instructions"]["value"]
        if total > 0:
            rate = metrics["branch_misses"]["value"] / total * 100.0
            metrics["branch_miss_rate"] = metric(round(rate, 4), "percent")

    # Derived: L1d miss rate
    if "L1_dcache_load_misses" in metrics and "L1_dcache_loads" in metrics:
        loads = metrics["L1_dcache_loads"]["value"]
        if loads > 0:
            rate = metrics["L1_dcache_load_misses"]["value"] / loads * 100.0
            metrics["l1d_miss_rate"] = metric(round(rate, 4), "percent")

    return metrics


def main() -> int:
    args = parse_args()
    try:
        text = args.input.read_text()
    except OSError as e:
        print(f"error reading input: {e}")
        return 1

    metrics = parse_perf_output(text)
    if not metrics:
        print("error: no perf stat metrics found in input")
        return 1

    config: dict = {}
    if args.command:
        config["command"] = args.command

    now = datetime.now(timezone.utc)
    result = make_envelope("perf_stat", config, metrics, tags=args.tags, timestamp=now)
    output_path = args.output or default_output_path("perf_stat", now)
    write_result(result, output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
