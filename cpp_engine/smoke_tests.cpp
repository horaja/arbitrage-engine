#include <filesystem>
#include <iostream>
#include <string>

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
    EngineConfig config;
    config.input_path = fixture_path("sample_replay.csv").string();
    config.quiet = true;
    config.replay_sleep_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "sample replay should succeed");
    all_passed &= require(summary.events_processed == 6, "sample replay should process 6 events");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("arb_fixture.csv").string();
    config.quiet = true;
    config.replay_sleep_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "arb fixture should succeed");
    all_passed &= require(summary.arbitrage_detections >= 1, "arb fixture should detect arbitrage");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("no_arb_fixture.csv").string();
    config.quiet = true;
    config.replay_sleep_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "no-arb fixture should succeed");
    all_passed &= require(summary.arbitrage_detections == 0, "no-arb fixture should not detect arbitrage");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("missing_fixture.csv").string();
    config.quiet = true;
    config.replay_sleep_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(!summary.succeeded, "missing file should fail cleanly");
    all_passed &= require(!summary.error_message.empty(), "missing file should return an error message");
  }

  if (!all_passed) {
    return 1;
  }

  std::cout << "All smoke tests passed.\n";
  return 0;
}
