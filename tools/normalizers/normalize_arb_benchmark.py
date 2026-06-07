#!/usr/bin/env python3
"""Normalize arb_benchmark --output-json into canonical format."""

import argparse
import json
from datetime import datetime, timezone
from pathlib import Path

from common import default_output_path, make_envelope, metric, write_result


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert arb_benchmark JSON to canonical benchmark format."
    )
    parser.add_argument("input", type=Path, help="Path to arb_benchmark JSON output.")
    parser.add_argument("--output", type=Path, help="Output path (default: auto-generated).")
    parser.add_argument("--tags", nargs="*", default=[], help="Optional tags.")
    return parser.parse_args()


def normalize(report: dict) -> dict[str, dict]:
    """Extract canonical metrics from arb_benchmark JSON."""
    agg = report["aggregate"]
    metrics: dict[str, dict] = {}

    # Throughput
    metrics["throughput_events_per_second_mean"] = metric(agg["events_per_second"]["mean"], "events/s")
    metrics["throughput_events_per_second_min"] = metric(agg["events_per_second"]["min"], "events/s")
    metrics["throughput_events_per_second_max"] = metric(agg["events_per_second"]["max"], "events/s")
    metrics["producer_events_per_second_mean"] = metric(agg["producer_events_per_second"]["mean"], "events/s")
    metrics["consumer_events_per_second_mean"] = metric(agg["consumer_events_per_second"]["mean"], "events/s")

    # Timing
    metrics["elapsed_seconds_mean"] = metric(agg["elapsed_seconds"]["mean"], "s")
    metrics["producer_elapsed_seconds_mean"] = metric(agg["producer_elapsed_seconds"]["mean"], "s")
    metrics["consumer_elapsed_seconds_mean"] = metric(agg["consumer_elapsed_seconds"]["mean"], "s")

    # Events
    metrics["events_processed_mean"] = metric(agg["events_processed"]["mean"], "count")
    metrics["arbitrage_detections_mean"] = metric(agg["arbitrage_detections"]["mean"], "count")

    # Logic latency
    logic = agg["logic_latency"]
    for key in ("min_ns", "avg_ns", "p50_ns", "p95_ns", "p99_ns", "max_ns"):
        short = key.replace("_ns", "")
        metrics[f"logic_latency_{short}_ns"] = metric(logic[key], "ns")

    # Stage latencies
    stages = agg["stage_latencies"]
    for stage_name, stage_data in stages.items():
        for key in ("min_ns", "avg_ns", "p50_ns", "p95_ns", "p99_ns", "max_ns"):
            short = key.replace("_ns", "")
            metrics[f"stage_{stage_name}_{short}_ns"] = metric(stage_data[key], "ns")

    # Queue depth
    metrics["queue_depth_avg_mean"] = metric(agg["avg_queue_depth"]["mean"], "count")
    metrics["queue_depth_p95_mean"] = metric(agg["p95_queue_depth"]["mean"], "count")
    metrics["queue_depth_max_max"] = metric(agg["max_queue_depth"]["max"], "count")

    return metrics


def main() -> int:
    args = parse_args()
    try:
        with args.input.open() as f:
            report = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"error reading input: {e}")
        return 1

    meta = report.get("metadata", {})
    config = {
        "input_path": meta.get("input_path", ""),
        "repeat_count": meta.get("repeat_count", 0),
        "warmup_events": meta.get("warmup_events", 0),
        "fee_bps": meta.get("fee_bps", 0.0),
        "symbol_count": meta.get("symbol_count", 0),
        "input_event_count": meta.get("input_event_count", 0),
    }

    metrics = normalize(report)
    now = datetime.now(timezone.utc)
    result = make_envelope("arb_benchmark", config, metrics, tags=args.tags, timestamp=now)

    output_path = args.output or default_output_path("arb_benchmark", now)
    write_result(result, output_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
