#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "benchmark_metrics.h"
#include "replay_runner.h"

struct BenchmarkRunResult {
  std::size_t run_index = 0;
  RunSummary summary;
  double events_per_second = 0.0;
};

struct NumericSummary {
  double mean = 0.0;
  double min = 0.0;
  double max = 0.0;
};

struct BenchmarkSummary {
  std::size_t repeat_count = 0;
  NumericSummary total_events_seen;
  NumericSummary events_processed;
  NumericSummary elapsed_seconds;
  NumericSummary events_per_second;
  NumericSummary arbitrage_detections;
  NumericSummary max_queue_depth;
  LatencySummary logic_latency;
  StageLatencySummary stage_latencies;
};

struct BenchmarkReportMetadata {
  std::string input_path;
  std::size_t repeat_count = 0;
  std::size_t warmup_events = 0;
  double fee_bps = 0.0;
  std::size_t symbol_count = 0;
  std::size_t input_event_count = 0;
};

double compute_events_per_second(const RunSummary& summary);
BenchmarkSummary summarize_benchmark_runs(
    const std::vector<BenchmarkRunResult>& runs,
    const BenchmarkSamples& aggregate_samples);
bool write_benchmark_report_json(
    const std::string& output_path,
    const BenchmarkReportMetadata& metadata,
    const BenchmarkSummary& aggregate,
    const std::vector<BenchmarkRunResult>& runs,
    std::string& error_message);
