#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "clock.h"
#include "market_event.h"

struct OpportunitySummary {
  std::vector<std::string> cycle;
  double gross_profit_percent = 0.0;
  double net_profit_percent = 0.0;
  double max_executable_size = 0.0;
  std::string anchor_currency;
};

struct LatencySummary {
  std::uint64_t min_ns = 0;
  std::uint64_t avg_ns = 0;
  std::uint64_t p50_ns = 0;
  std::uint64_t p95_ns = 0;
  std::uint64_t p99_ns = 0;
  std::uint64_t max_ns = 0;
};

struct EngineConfig {
  std::string input_path;
  int replay_delay_ms = 0;
  ClockMode clock_mode = ClockMode::ReplayTime;
  bool quiet = false;
  double fee_bps = 0.0;
};

struct RunSummary {
  bool succeeded = false;
  std::string error_message;
  std::size_t events_processed = 0;
  std::size_t arbitrage_detections = 0;
  std::size_t max_queue_depth = 0;
  double elapsed_seconds = 0.0;
  LatencySummary logic_latency;
  std::optional<OpportunitySummary> last_opportunity;
};

class ReplayRunner {
public:
  explicit ReplayRunner(EngineConfig config);
  RunSummary run() const;

private:
  EngineConfig config_;
};
