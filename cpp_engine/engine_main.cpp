#include <iostream>
#include <string>

#include "replay_runner.h"

namespace {

void print_usage() {
  std::cout << "Usage: arb_engine --input <csv-path> [--sleep-ms <n>] [--quiet]\n";
}

bool parse_non_negative_int(const std::string& value, int& parsed_value) {
  try {
    parsed_value = std::stoi(value);
  } catch (const std::exception&) {
    return false;
  }

  return parsed_value >= 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  EngineConfig config;

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];

    if (argument == "--help") {
      print_usage();
      return 0;
    }

    if (argument == "--quiet") {
      config.quiet = true;
      continue;
    }

    if (argument == "--input" && index + 1 < argc) {
      config.input_path = argv[++index];
      continue;
    }

    if (argument == "--sleep-ms" && index + 1 < argc) {
      if (!parse_non_negative_int(argv[++index], config.replay_delay_ms)) {
        std::cerr << "Invalid value for --sleep-ms.\n";
        print_usage();
        return 1;
      }
      if (config.replay_delay_ms > 0) {
        config.clock_mode = ClockMode::WallTime;
      }
      continue;
    }

    std::cerr << "Unknown or incomplete argument: " << argument << "\n";
    print_usage();
    return 1;
  }

  if (config.input_path.empty()) {
    print_usage();
    return 1;
  }

  ReplayRunner runner(config);
  const RunSummary summary = runner.run();

  if (!summary.succeeded) {
    std::cerr << "Replay failed: " << summary.error_message << "\n";
    return 1;
  }

  std::cout << "Replay completed. Events processed: " << summary.events_processed
            << ", opportunities detected: " << summary.arbitrage_detections << "\n";
  return 0;
}
