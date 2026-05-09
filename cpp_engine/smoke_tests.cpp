#include <filesystem>
#include <iostream>
#include <string>

#include "replay_adapter.h"
#include "replay_runner.h"

namespace {

std::filesystem::path fixture_path(const std::string& fixture_name) {
  return std::filesystem::path(ARB_REPO_ROOT) / "fixtures" / fixture_name;
}

bool require(bool condition, const std::string& message) {
  if (condition) {
    return true;
  }

  std::cerr << "FAILED: " << message << "\n";
  return false;
}

}  // namespace

int main() {
  bool all_passed = true;

  {
    CsvReplayAdapter adapter(fixture_path("sample_replay.csv").string());
    std::string error_message;
    all_passed &= require(adapter.open(error_message), "sample adapter should open");
    all_passed &= require(error_message.empty(), "sample adapter should not return an error");
    all_passed &= require(adapter.metadata().event_count == 6, "sample adapter should count 6 events");
    all_passed &= require(adapter.metadata().symbols.size() == 3, "sample adapter should track 3 symbols");

    MarketEvent first_event;
    all_passed &= require(adapter.next_event(first_event, error_message), "sample adapter should emit the first event");
    all_passed &= require(first_event.exchange_timestamp == "2026-01-01T00:00:00Z", "first event exchange timestamp should match");
    all_passed &= require(first_event.receive_timestamp == first_event.exchange_timestamp, "first event receive timestamp should match exchange timestamp");
    all_passed &= require(first_event.event_type == MarketEventType::TradeTick, "first event should be a trade tick");
    all_passed &= require(first_event.symbol == "BTC-USD", "first event symbol should match");
    all_passed &= require(first_event.trade_tick.has_value(), "first event should carry a trade payload");
    all_passed &= require(first_event.trade_tick->price == 50000.0, "first event price should match");
    all_passed &= require(first_event.trade_tick->quantity == 0.25, "first event quantity should match");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("sample_replay.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "sample replay should succeed");
    all_passed &= require(summary.events_processed == 6, "sample replay should process 6 events");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("arb_fixture.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "arb fixture should succeed");
    all_passed &= require(summary.arbitrage_detections >= 1, "arb fixture should detect arbitrage");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("no_arb_fixture.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "no-arb fixture should succeed");
    all_passed &= require(summary.arbitrage_detections == 0, "no-arb fixture should not detect arbitrage");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("missing_fixture.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(!summary.succeeded, "missing file should fail cleanly");
    all_passed &= require(!summary.error_message.empty(), "missing file should return an error message");
  }

  {
    CsvReplayAdapter adapter(fixture_path("malformed_replay.csv").string());
    std::string error_message;
    all_passed &= require(!adapter.open(error_message), "malformed replay should fail during adapter open");
    all_passed &= require(!error_message.empty(), "malformed replay should return an adapter error");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("sample_replay.csv").string();
    config.quiet = true;
    config.clock_mode = ClockMode::WallTime;
    config.replay_delay_ms = 1;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "wall-time replay should succeed");
    all_passed &= require(summary.events_processed == 6, "wall-time replay should still process 6 events");
  }

  if (!all_passed) {
    return 1;
  }

  std::cout << "All smoke tests passed.\n";
  return 0;
}
