#include "benchmark_metrics.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

void append_latency_vector(
    std::vector<std::uint64_t>& destination,
    std::vector<std::uint64_t>& source) {
  destination.insert(
      destination.end(),
      std::make_move_iterator(source.begin()),
      std::make_move_iterator(source.end()));
  source.clear();
}

}  // namespace

LatencySummary summarize_latencies(const std::vector<std::uint64_t>& latencies_ns) {
  LatencySummary summary;
  if (latencies_ns.empty()) {
    return summary;
  }

  std::vector<std::uint64_t> sorted_latencies = latencies_ns;
  std::sort(sorted_latencies.begin(), sorted_latencies.end());

  const auto percentile_value = [&](double percentile) {
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(percentile * static_cast<double>(sorted_latencies.size())) - 1.0);
    return sorted_latencies[std::min(index, sorted_latencies.size() - 1)];
  };

  std::uint64_t total_ns = 0;
  for (const std::uint64_t latency_ns : sorted_latencies) {
    total_ns += latency_ns;
  }

  summary.min_ns = sorted_latencies.front();
  summary.avg_ns = total_ns / sorted_latencies.size();
  summary.p50_ns = percentile_value(0.50);
  summary.p95_ns = percentile_value(0.95);
  summary.p99_ns = percentile_value(0.99);
  summary.max_ns = sorted_latencies.back();
  return summary;
}

StageLatencySummary summarize_stage_latencies(const StageLatencySamples& samples) {
  StageLatencySummary summary;
  summary.adapter_next_event = summarize_latencies(samples.adapter_next_event_ns);
  summary.book_apply_quote = summarize_latencies(samples.book_apply_quote_ns);
  summary.graph_update_quote = summarize_latencies(samples.graph_update_quote_ns);
  summary.cycle_detection = summarize_latencies(samples.cycle_detection_ns);
  summary.opportunity_compute = summarize_latencies(samples.opportunity_compute_ns);
  summary.pipeline_backlog_latency = summarize_latencies(samples.pipeline_backlog_latency_ns);
  return summary;
}

void append_benchmark_samples(BenchmarkSamples& destination, BenchmarkSamples&& source) {
  append_latency_vector(destination.logic_latency_ns, source.logic_latency_ns);
  append_latency_vector(
      destination.stage_latencies.adapter_next_event_ns,
      source.stage_latencies.adapter_next_event_ns);
  append_latency_vector(
      destination.stage_latencies.book_apply_quote_ns,
      source.stage_latencies.book_apply_quote_ns);
  append_latency_vector(
      destination.stage_latencies.graph_update_quote_ns,
      source.stage_latencies.graph_update_quote_ns);
  append_latency_vector(
      destination.stage_latencies.cycle_detection_ns,
      source.stage_latencies.cycle_detection_ns);
  append_latency_vector(
      destination.stage_latencies.opportunity_compute_ns,
      source.stage_latencies.opportunity_compute_ns);
  append_latency_vector(
      destination.stage_latencies.pipeline_backlog_latency_ns,
      source.stage_latencies.pipeline_backlog_latency_ns);
}
