#!/usr/bin/env python3

import argparse
import json
from pathlib import Path

import matplotlib


matplotlib.use("Agg")
import matplotlib.pyplot as plt


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Plot arb_benchmark JSON output."
    )
    parser.add_argument("inputs", nargs="+", help="Benchmark JSON files or directories.")
    parser.add_argument("--output-dir", type=Path, default=Path("benchmark_plots"))
    return parser.parse_args()


def collect_json_paths(inputs: list[str]) -> list[Path]:
    json_paths: list[Path] = []
    for raw_input in inputs:
        path = Path(raw_input)
        if path.is_dir():
            json_paths.extend(sorted(candidate for candidate in path.iterdir() if candidate.suffix == ".json"))
        else:
            json_paths.append(path)
    return json_paths


def load_reports(paths: list[Path]) -> list[dict]:
    reports: list[dict] = []
    for path in paths:
        with path.open() as handle:
            report = json.load(handle)
        report["_path"] = str(path)
        reports.append(report)
    return reports


def backlog_latency_summary(report: dict) -> dict:
    stage_latencies = report["aggregate"]["stage_latencies"]
    if "pipeline_backlog_latency" in stage_latencies:
        return stage_latencies["pipeline_backlog_latency"]
    return stage_latencies["queue_residence_latency"]


def stage_avg_ns(report: dict) -> tuple[list[str], list[float]]:
    stage_latencies = report["aggregate"]["stage_latencies"]
    names = [
        "adapter_next_event",
        "book_apply_quote",
        "graph_update_quote",
        "cycle_detection",
        "opportunity_compute",
    ]
    values = [stage_latencies[name]["avg_ns"] for name in names]
    return names, values


def plot_stage_latency_bar(report: dict, output_dir: Path) -> None:
    names, values = stage_avg_ns(report)
    plt.figure(figsize=(10, 5))
    plt.bar(names, values, color="#1f77b4")
    plt.ylabel("Average latency (ns)")
    plt.title("Stage Latency Averages")
    plt.xticks(rotation=30, ha="right")
    plt.tight_layout()
    plt.savefig(output_dir / "stage_latency_bar.png")
    plt.close()


def plot_pipeline_backlog_latency(report: dict, output_dir: Path) -> None:
    backlog = backlog_latency_summary(report)
    labels = ["p50", "p95", "p99"]
    values = [backlog["p50_ns"], backlog["p95_ns"], backlog["p99_ns"]]
    plt.figure(figsize=(6, 4))
    plt.bar(labels, values, color="#9467bd")
    plt.ylabel("Latency (ns)")
    plt.title("Pipeline Backlog Latency Percentiles")
    plt.tight_layout()
    plt.savefig(output_dir / "pipeline_backlog_latency.png")
    plt.close()


def plot_latency_percentiles(report: dict, output_dir: Path) -> None:
    logic = report["aggregate"]["logic_latency"]
    labels = ["p50", "p95", "p99"]
    values = [logic["p50_ns"], logic["p95_ns"], logic["p99_ns"]]
    plt.figure(figsize=(6, 4))
    plt.bar(labels, values, color="#ff7f0e")
    plt.ylabel("Latency (ns)")
    plt.title("Logic Latency Percentiles")
    plt.tight_layout()
    plt.savefig(output_dir / "latency_percentiles.png")
    plt.close()


def plot_queue_depth_summary(report: dict, output_dir: Path) -> None:
    runs = report["runs"]
    labels = [str(run["run_index"]) for run in runs]
    values = [run["max_queue_depth"] for run in runs]
    plt.figure(figsize=(7, 4))
    plt.bar(labels, values, color="#2ca02c")
    plt.xlabel("Run")
    plt.ylabel("Max queue depth")
    plt.title("Queue Depth by Run")
    plt.tight_layout()
    plt.savefig(output_dir / "queue_depth_summary.png")
    plt.close()


def plot_throughput_by_symbol_count(reports: list[dict], output_dir: Path) -> None:
    points = sorted(
        (
            report["metadata"]["symbol_count"],
            report["aggregate"]["events_per_second"]["mean"],
        )
        for report in reports
    )
    distinct_symbol_counts = {symbol_count for symbol_count, _ in points}
    if len(distinct_symbol_counts) < 2:
        return

    plt.figure(figsize=(7, 4))
    plt.plot(
        [symbol_count for symbol_count, _ in points],
        [events_per_second for _, events_per_second in points],
        marker="o",
        color="#d62728",
    )
    plt.xlabel("Symbol count")
    plt.ylabel("Mean events/sec")
    plt.title("Throughput by Symbol Count")
    plt.xscale("log")
    plt.tight_layout()
    plt.savefig(output_dir / "throughput_by_symbol_count.png")
    plt.close()


def main() -> int:
    args = parse_args()
    try:
        json_paths = collect_json_paths(args.inputs)
        if not json_paths:
            print("error: no benchmark JSON files found")
            return 1

        reports = load_reports(json_paths)
        output_dir = args.output_dir
        output_dir.mkdir(parents=True, exist_ok=True)

        first_report = reports[0]
        plot_stage_latency_bar(first_report, output_dir)
        plot_pipeline_backlog_latency(first_report, output_dir)
        plot_latency_percentiles(first_report, output_dir)
        plot_queue_depth_summary(first_report, output_dir)
        plot_throughput_by_symbol_count(reports, output_dir)
    except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError) as error:
        print(f"error: {error}")
        return 1

    print(f"generated plots in {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
