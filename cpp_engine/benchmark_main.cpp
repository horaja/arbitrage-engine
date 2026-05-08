#include <iomanip>
#include <iostream>
#include <string>

#include "replay_runner.h"

namespace {

void print_usage() {
  std::cout << "Usage: arb_benchmark --input <csv-path>\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  EngineConfig config;
  config.quiet = true;
  config.replay_sleep_ms = 0;

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];

    if (argument == "--help") {
      print_usage();
      return 0;
    }

    if (argument == "--input" && index + 1 < argc) {
      config.input_path = argv[++index];
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
    std::cerr << "Benchmark failed: " << summary.error_message << "\n";
    return 1;
  }

  const double events_per_second =
      summary.elapsed_seconds > 0.0
          ? static_cast<double>(summary.events_processed) / summary.elapsed_seconds
          : 0.0;

  std::cout << "Benchmark Summary\n";
  std::cout << "events_processed: " << summary.events_processed << "\n";
  std::cout << "arbitrage_detections: " << summary.arbitrage_detections << "\n";
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "elapsed_seconds: " << summary.elapsed_seconds << "\n";
  std::cout << std::setprecision(2);
  std::cout << "events_per_second: " << events_per_second << "\n";
  std::cout << "max_queue_depth: " << summary.max_queue_depth << "\n";
  std::cout << "logic_latency_ns: min=" << summary.logic_latency.min_ns
            << " avg=" << summary.logic_latency.avg_ns
            << " p50=" << summary.logic_latency.p50_ns
            << " p95=" << summary.logic_latency.p95_ns
            << " p99=" << summary.logic_latency.p99_ns
            << " max=" << summary.logic_latency.max_ns << "\n";
  std::cout << "allocation_count: deferred\n";

  return 0;
}
