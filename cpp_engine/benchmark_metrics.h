#pragma once

#include <cstdint>
#include <vector>

struct LatencySummary {
  std::uint64_t min_ns = 0;
  std::uint64_t avg_ns = 0;
  std::uint64_t p50_ns = 0;
  std::uint64_t p95_ns = 0;
  std::uint64_t p99_ns = 0;
  std::uint64_t max_ns = 0;
};

struct StageLatencySummary {
  LatencySummary adapter_next_event;
  LatencySummary book_apply_quote;
  LatencySummary graph_update_quote;
  LatencySummary cycle_detection;
  LatencySummary opportunity_compute;
  LatencySummary pipeline_backlog_latency;
};

struct StageLatencySamples {
  std::vector<std::uint64_t> adapter_next_event_ns;
  std::vector<std::uint64_t> book_apply_quote_ns;
  std::vector<std::uint64_t> graph_update_quote_ns;
  std::vector<std::uint64_t> cycle_detection_ns;
  std::vector<std::uint64_t> opportunity_compute_ns;
  std::vector<std::uint64_t> pipeline_backlog_latency_ns;
};

struct BenchmarkSamples {
  std::vector<std::uint64_t> logic_latency_ns;
  StageLatencySamples stage_latencies;
};

LatencySummary summarize_latencies(const std::vector<std::uint64_t>& latencies_ns);
StageLatencySummary summarize_stage_latencies(const StageLatencySamples& samples);
void append_benchmark_samples(BenchmarkSamples& destination, BenchmarkSamples&& source);
