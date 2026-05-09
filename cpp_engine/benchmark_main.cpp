#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "benchmark_report.h"
#include "replay_adapter.h"
#include "replay_runner.h"

namespace {

void print_usage() {
  std::cout << "Usage: arb_benchmark --input <csv-path> [--repeat <n>] "
               "[--warmup-events <n>] [--output-json <path>]\n";
}

bool parse_positive_size(const std::string& value, std::size_t& parsed_value) {
  try {
    const unsigned long long parsed = std::stoull(value);
    if (parsed == 0) {
      return false;
    }
    parsed_value = static_cast<std::size_t>(parsed);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool parse_size(const std::string& value, std::size_t& parsed_value) {
  try {
    parsed_value = static_cast<std::size_t>(std::stoull(value));
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

void print_run_summary(const BenchmarkRunResult& run) {
  std::cout << "run " << run.run_index
            << ": events_processed=" << run.summary.events_processed
            << " producer_elapsed_seconds=" << std::fixed << std::setprecision(6)
            << run.summary.producer_elapsed_seconds
            << " consumer_elapsed_seconds=" << run.summary.consumer_elapsed_seconds
            << " elapsed_seconds=" << std::fixed << std::setprecision(6)
            << run.summary.elapsed_seconds
            << " producer_events_per_second=" << std::setprecision(2)
            << run.summary.producer_events_per_second
            << " consumer_events_per_second=" << run.summary.consumer_events_per_second
            << " events_per_second=" << std::setprecision(2)
            << run.events_per_second
            << " avg_queue_depth=" << std::fixed << std::setprecision(2)
            << run.summary.queue_depth.avg_depth
            << " p95_queue_depth=" << std::setprecision(0)
            << static_cast<double>(run.summary.queue_depth.p95_depth)
            << " max_queue_depth=" << run.summary.max_queue_depth
            << " arbitrage_detections=" << run.summary.arbitrage_detections
            << "\n";
}

void print_aggregate_summary(const BenchmarkSummary& summary) {
  std::cout << "Benchmark Summary\n";
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "events_processed_mean: " << summary.events_processed.mean << "\n";
  std::cout << std::setprecision(6);
  std::cout << "producer_elapsed_seconds_mean: " << summary.producer_elapsed_seconds.mean << "\n";
  std::cout << "consumer_elapsed_seconds_mean: " << summary.consumer_elapsed_seconds.mean << "\n";
  std::cout << "elapsed_seconds_mean: " << summary.elapsed_seconds.mean << "\n";
  std::cout << std::setprecision(2);
  std::cout << "producer_events_per_second_mean: " << summary.producer_events_per_second.mean << "\n";
  std::cout << "consumer_events_per_second_mean: " << summary.consumer_events_per_second.mean << "\n";
  std::cout << "events_per_second_mean: " << summary.events_per_second.mean << "\n";
  std::cout << "avg_queue_depth_mean: " << summary.avg_queue_depth.mean << "\n";
  std::cout << "p95_queue_depth_mean: " << summary.p95_queue_depth.mean << "\n";
  std::cout << "max_queue_depth_mean: " << summary.max_queue_depth.mean << "\n";
  std::cout << "max_queue_depth_max: " << summary.max_queue_depth.max << "\n";
  std::cout << "arbitrage_detections_mean: " << summary.arbitrage_detections.mean << "\n";
  std::cout << "logic_latency_ns: min=" << summary.logic_latency.min_ns
            << " avg=" << summary.logic_latency.avg_ns
            << " p50=" << summary.logic_latency.p50_ns
            << " p95=" << summary.logic_latency.p95_ns
            << " p99=" << summary.logic_latency.p99_ns
            << " max=" << summary.logic_latency.max_ns << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  EngineConfig config;
  config.quiet = true;
  config.replay_delay_ms = 0;
  config.clock_mode = ClockMode::ReplayTime;

  std::size_t repeat_count = 1;
  std::string output_json_path;

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

    if (argument == "--repeat" && index + 1 < argc) {
      if (!parse_positive_size(argv[++index], repeat_count)) {
        std::cerr << "Invalid value for --repeat.\n";
        print_usage();
        return 1;
      }
      continue;
    }

    if (argument == "--warmup-events" && index + 1 < argc) {
      if (!parse_size(argv[++index], config.warmup_events)) {
        std::cerr << "Invalid value for --warmup-events.\n";
        print_usage();
        return 1;
      }
      continue;
    }

    if (argument == "--output-json" && index + 1 < argc) {
      output_json_path = argv[++index];
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

  CsvReplayAdapter metadata_adapter(config.input_path);
  std::string metadata_error;
  if (!metadata_adapter.open(metadata_error)) {
    std::cerr << "Benchmark failed: " << metadata_error << "\n";
    return 1;
  }

  BenchmarkReportMetadata metadata;
  metadata.input_path = config.input_path;
  metadata.repeat_count = repeat_count;
  metadata.warmup_events = config.warmup_events;
  metadata.fee_bps = config.fee_bps;
  metadata.symbol_count = metadata_adapter.metadata().symbols.size();
  metadata.input_event_count = metadata_adapter.metadata().event_count;

  std::vector<BenchmarkRunResult> runs;
  runs.reserve(repeat_count);
  BenchmarkSamples aggregate_samples;

  for (std::size_t run_index = 1; run_index <= repeat_count; ++run_index) {
    ReplayRunner runner(config);
    RunSummary summary = runner.run();
    if (!summary.succeeded) {
      std::cerr << "Benchmark failed on run " << run_index << ": " << summary.error_message << "\n";
      return 1;
    }

    append_benchmark_samples(aggregate_samples, std::move(summary.benchmark_samples));
    BenchmarkRunResult run;
    run.run_index = run_index;
    run.events_per_second = compute_events_per_second(summary);
    run.summary = std::move(summary);
    print_run_summary(run);
    runs.push_back(std::move(run));
  }

  const BenchmarkSummary aggregate = summarize_benchmark_runs(runs, aggregate_samples);
  print_aggregate_summary(aggregate);

  if (!output_json_path.empty()) {
    std::string json_error;
    if (!write_benchmark_report_json(output_json_path, metadata, aggregate, runs, json_error)) {
      std::cerr << "Benchmark failed: " << json_error << "\n";
      return 1;
    }
    std::cout << "benchmark_json: " << output_json_path << "\n";
  }

  return 0;
}
