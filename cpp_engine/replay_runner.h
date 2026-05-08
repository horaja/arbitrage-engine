#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct MarketEvent {
  std::string exchange_timestamp;
  std::string symbol;
  double price = 0.0;
  double quantity = 0.0;
};

struct OpportunitySummary {
  std::vector<std::string> cycle;
  double profit_percent = 0.0;
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
  int replay_sleep_ms = 5;
  bool quiet = false;
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

class CsvReplaySource {
public:
  struct Metadata {
    std::vector<std::string> symbols;
    std::size_t event_count = 0;
  };

  static std::optional<Metadata> inspect(const std::string& input_path, std::string& error_message);
};
