#!/usr/bin/env python3
"""Normalize valgrind --tool=cachegrind output into canonical format.

Usage:
  valgrind --tool=cachegrind --cachegrind-out-file=cachegrind.out \
    ./build/arb_benchmark --input fixtures/sample_replay.csv

  cg_annotate cachegrind.out > cachegrind_annotated.txt

  python3 tools/normalizers/normalize_cachegrind.py cachegrind_annotated.txt
"""

import argparse
import re
from datetime import datetime, timezone
from pathlib import Path

from common import default_output_path, make_envelope, metric, write_result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert cg_annotate output to canonical benchmark format."
    )
    parser.add_argument("input", type=Path, help="Path to cg_annotate output.")
    parser.add_argument("--command", type=str, default="", help="The command that was profiled.")
    parser.add_argument("--output", type=Path, help="Output path (default: auto-generated).")
    parser.add_argument("--tags", nargs="*", default=[], help="Optional tags.")
    return parser.parse_args()


def parse_number(s: str) -> int:
    return int(s.replace(",", ""))


# cg_annotate summary lines look like:
# I   refs:      12,345,678
# I1  misses:       123,456
# LLi misses:        12,345
# D   refs:       9,876,543  (6,543,210 rd   + 3,333,333 wr)
# D1  misses:       234,567  (  200,000 rd   +    34,567 wr)
# LLd misses:        23,456  (   20,000 rd   +     3,456 wr)
# LL  refs:         358,023  (  323,456 rd   +    34,567 wr)
# LL  misses:        35,801  (   32,345 rd   +     3,456 wr)

SUMMARY_RE = re.compile(
    r"^\s*(?P<label>[\w]+\s*[\w]*)\s*(?:refs|misses):\s*(?P<total>[\d,]+)"
    r"(?:\s*\(\s*(?P<rd>[\d,]+)\s*rd\s*\+\s*(?P<wr>[\d,]+)\s*wr\s*\))?"
)

# Simpler pattern for lines like:
# Ir    12,345,678
FIELD_RE = re.compile(r"^(?P<label>[A-Z]\w*)\s+(?P<value>[\d,]+)")


def parse_cg_annotate(text: str) -> dict[str, dict]:
    metrics: dict[str, dict] = {}

    for line in text.splitlines():
        m = SUMMARY_RE.match(line)
        if not m:
            continue

        label = m.group("label").strip()
        total = parse_number(m.group("total"))

        if "refs" in line:
            suffix = "refs"
            unit = "refs"
        else:
            suffix = "misses"
            unit = "count"

        key = label.replace(" ", "").lower()
        metrics[f"{key}_{suffix}"] = metric(total, unit)

        if m.group("rd"):
            metrics[f"{key}_{suffix}_rd"] = metric(parse_number(m.group("rd")), unit)
        if m.group("wr"):
            metrics[f"{key}_{suffix}_wr"] = metric(parse_number(m.group("wr")), unit)

    # Derived miss rates
    if "i_refs" in metrics and "i1_misses" in metrics:
        refs = metrics["i_refs"]["value"]
        if refs > 0:
            rate = metrics["i1_misses"]["value"] / refs * 100.0
            metrics["i1_miss_rate"] = metric(round(rate, 4), "percent")

    if "d_refs" in metrics and "d1_misses" in metrics:
        refs = metrics["d_refs"]["value"]
        if refs > 0:
            rate = metrics["d1_misses"]["value"] / refs * 100.0
            metrics["d1_miss_rate"] = metric(round(rate, 4), "percent")

    if "ll_refs" in metrics and "ll_misses" in metrics:
        refs = metrics["ll_refs"]["value"]
        if refs > 0:
            rate = metrics["ll_misses"]["value"] / refs * 100.0
            metrics["ll_miss_rate"] = metric(round(rate, 4), "percent")

    return metrics


def main() -> int:
    args = parse_args()
    try:
        text = args.input.read_text()
    except OSError as e:
        print(f"error reading input: {e}")
        return 1

    metrics = parse_cg_annotate(text)
    if not metrics:
        print("error: no cachegrind metrics found in input")
        return 1

    config: dict = {}
    if args.command:
        config["command"] = args.command

    now = datetime.now(timezone.utc)
    result = make_envelope("cachegrind", config, metrics, tags=args.tags, timestamp=now)
    output_path = args.output or default_output_path("cachegrind", now)
    write_result(result, output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
