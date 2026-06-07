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


# Modern cg_annotate emits a tabular format:
#
#   Events shown:     Ir I1mr ILmr Dr D1mr DLmr Dw D1mw DLmw
#   ...
#   2,901,047 (100.0%) 5,665 (100.0%) ... 13,220 (100.0%)  PROGRAM TOTALS
#
# Each event abbreviation maps to a canonical metric name. See:
# https://valgrind.org/docs/manual/cg-manual.html
EVENT_NAMES: dict[str, tuple[str, str]] = {
    "Ir": ("instructions", "count"),
    "I1mr": ("l1i_read_misses", "count"),
    "ILmr": ("lli_read_misses", "count"),
    "Dr": ("data_reads", "refs"),
    "D1mr": ("l1d_read_misses", "count"),
    "DLmr": ("lld_read_misses", "count"),
    "Dw": ("data_writes", "refs"),
    "D1mw": ("l1d_write_misses", "count"),
    "DLmw": ("lld_write_misses", "count"),
    "Bc": ("conditional_branches", "count"),
    "Bcm": ("conditional_branch_mispredicts", "count"),
    "Bi": ("indirect_branches", "count"),
    "Bim": ("indirect_branch_mispredicts", "count"),
}

EVENTS_SHOWN_RE = re.compile(r"^Events shown:\s*(?P<events>.+)$")
# Numbers in the PROGRAM TOTALS row, each followed by a "(pct%)" group.
TOTALS_VALUE_RE = re.compile(r"([\d,]+)\s*\(\s*[\d.]+%\)")

# Legacy summary format (older valgrind):
# D1  misses:       234,567  (  200,000 rd   +    34,567 wr)
LEGACY_RE = re.compile(
    r"^\s*(?P<label>[\w]+\s*[\w]*)\s*(?:refs|misses):\s*(?P<total>[\d,]+)"
    r"(?:\s*\(\s*(?P<rd>[\d,]+)\s*rd\s*\+\s*(?P<wr>[\d,]+)\s*wr\s*\))?"
)


def add_derived_rates(values: dict[str, int], metrics: dict[str, dict]) -> None:
    """Compute standard cachegrind miss rates from raw event counts."""
    ir = values.get("Ir", 0)
    dr = values.get("Dr", 0)
    dw = values.get("Dw", 0)
    data_refs = dr + dw

    if ir > 0 and "I1mr" in values:
        metrics["l1i_miss_rate"] = metric(round(values["I1mr"] / ir * 100.0, 4), "percent")
    if ir > 0 and "ILmr" in values:
        metrics["lli_miss_rate"] = metric(round(values["ILmr"] / ir * 100.0, 4), "percent")

    if data_refs > 0 and "D1mr" in values and "D1mw" in values:
        d1_misses = values["D1mr"] + values["D1mw"]
        metrics["l1d_misses"] = metric(d1_misses, "count")
        metrics["l1d_miss_rate"] = metric(round(d1_misses / data_refs * 100.0, 4), "percent")
    if data_refs > 0 and "DLmr" in values and "DLmw" in values:
        dl_misses = values["DLmr"] + values["DLmw"]
        metrics["lld_misses"] = metric(dl_misses, "count")
        metrics["lld_miss_rate"] = metric(round(dl_misses / data_refs * 100.0, 4), "percent")

    # Combined last-level cache miss rate across instruction + data accesses.
    total_refs = ir + data_refs
    ll_misses = values.get("ILmr", 0) + values.get("DLmr", 0) + values.get("DLmw", 0)
    if total_refs > 0 and ("ILmr" in values or "DLmr" in values):
        metrics["ll_misses"] = metric(ll_misses, "count")
        metrics["ll_miss_rate"] = metric(round(ll_misses / total_refs * 100.0, 4), "percent")


def parse_tabular(text: str) -> dict[str, dict]:
    """Parse the modern tabular cg_annotate format."""
    lines = text.splitlines()

    event_order: list[str] = []
    for line in lines:
        m = EVENTS_SHOWN_RE.match(line.strip())
        if m:
            event_order = m.group("events").split()
            break

    totals_line = next((line for line in lines if "PROGRAM TOTALS" in line), None)
    if not event_order or totals_line is None:
        return {}

    raw_values = [parse_number(v) for v in TOTALS_VALUE_RE.findall(totals_line)]
    if len(raw_values) != len(event_order):
        return {}

    values: dict[str, int] = dict(zip(event_order, raw_values))

    metrics: dict[str, dict] = {}
    for abbrev, count in values.items():
        name, unit = EVENT_NAMES.get(abbrev, (abbrev.lower(), "count"))
        metrics[name] = metric(count, unit)

    add_derived_rates(values, metrics)
    return metrics


def parse_legacy(text: str) -> dict[str, dict]:
    """Parse the older 'I refs: ...' summary format."""
    metrics: dict[str, dict] = {}
    for line in text.splitlines():
        m = LEGACY_RE.match(line)
        if not m:
            continue
        label = m.group("label").strip()
        total = parse_number(m.group("total"))
        suffix = "refs" if "refs" in line else "misses"
        unit = "refs" if suffix == "refs" else "count"
        key = label.replace(" ", "").lower()
        metrics[f"{key}_{suffix}"] = metric(total, unit)
        if m.group("rd"):
            metrics[f"{key}_{suffix}_rd"] = metric(parse_number(m.group("rd")), unit)
        if m.group("wr"):
            metrics[f"{key}_{suffix}_wr"] = metric(parse_number(m.group("wr")), unit)
    return metrics


def parse_cg_annotate(text: str) -> dict[str, dict]:
    metrics = parse_tabular(text)
    if metrics:
        return metrics
    return parse_legacy(text)


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
