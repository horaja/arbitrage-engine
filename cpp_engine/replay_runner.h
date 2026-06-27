#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "benchmark_metrics.h"
#include "clock.h"
#include "market_event.h"

struct OpportunitySummary {
  std::vector<std::string> cycle;
  double gross_profit_percent = 0.0;
  double net_profit_percent = 0.0;
  double max_executable_size = 0.0;
  std::string anchor_currency;
};

struct EngineConfig {
  std::string input_path;
  int replay_delay_ms = 0;
  ClockMode clock_mode = ClockMode::ReplayTime;
  bool quiet = false;
  double fee_bps = 0.0;
  std::size_t warmup_events = 0;
  std::size_t queue_capacity = 4096;
};

struct RunSummary {
  bool succeeded = false;
  std::string error_message;
  std::size_t total_events_seen = 0;
  std::size_t events_processed = 0;
  std::size_t arbitrage_detections = 0;
  std::size_t max_queue_depth = 0;
  QueueDepthSummary queue_depth;
  double producer_elapsed_seconds = 0.0;
  double consumer_elapsed_seconds = 0.0;
  double producer_events_per_second = 0.0;
  double consumer_events_per_second = 0.0;
  double elapsed_seconds = 0.0;
  LatencySummary logic_latency;
  StageLatencySummary stage_latencies;
  BenchmarkSamples benchmark_samples;
  std::optional<OpportunitySummary> last_opportunity;
};

class ReplayRunner {
public:
  explicit ReplayRunner(EngineConfig config);
  RunSummary run() const;

private:
  EngineConfig config_;
};
