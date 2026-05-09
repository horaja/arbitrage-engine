#include "benchmark_report.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {

NumericSummary summarize_numeric_values(const std::vector<double>& values) {
  NumericSummary summary;
  if (values.empty()) {
    return summary;
  }

  double total = 0.0;
  double minimum = std::numeric_limits<double>::max();
  double maximum = std::numeric_limits<double>::lowest();
  for (double value : values) {
    total += value;
    if (value < minimum) {
      minimum = value;
    }
    if (value > maximum) {
      maximum = value;
    }
  }

  summary.mean = total / values.size();
  summary.min = minimum;
  summary.max = maximum;
  return summary;
}

std::string escape_json(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char character : value) {
    switch (character) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += character;
        break;
    }
  }
  return escaped;
}

void write_indent(std::ostream& output, int indent_level) {
  for (int index = 0; index < indent_level; ++index) {
    output << "  ";
  }
}

void write_latency_summary(
    std::ostream& output,
    const LatencySummary& summary,
    int indent_level) {
  write_indent(output, indent_level);
  output << "{\n";
  write_indent(output, indent_level + 1);
  output << "\"min_ns\": " << summary.min_ns << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"avg_ns\": " << summary.avg_ns << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"p50_ns\": " << summary.p50_ns << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"p95_ns\": " << summary.p95_ns << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"p99_ns\": " << summary.p99_ns << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"max_ns\": " << summary.max_ns << "\n";
  write_indent(output, indent_level);
  output << "}";
}

void write_stage_latency_summary(
    std::ostream& output,
    const StageLatencySummary& summary,
    int indent_level) {
  write_indent(output, indent_level);
  output << "{\n";
  write_indent(output, indent_level + 1);
  output << "\"adapter_next_event\": ";
  write_latency_summary(output, summary.adapter_next_event, 0);
  output << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"book_apply_quote\": ";
  write_latency_summary(output, summary.book_apply_quote, 0);
  output << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"graph_update_quote\": ";
  write_latency_summary(output, summary.graph_update_quote, 0);
  output << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"cycle_detection\": ";
  write_latency_summary(output, summary.cycle_detection, 0);
  output << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"opportunity_compute\": ";
  write_latency_summary(output, summary.opportunity_compute, 0);
  output << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"pipeline_backlog_latency\": ";
  write_latency_summary(output, summary.pipeline_backlog_latency, 0);
  output << "\n";
  write_indent(output, indent_level);
  output << "}";
}

void write_numeric_summary(
    std::ostream& output,
    const NumericSummary& summary,
    int indent_level) {
  write_indent(output, indent_level);
  output << "{\n";
  write_indent(output, indent_level + 1);
  output << "\"mean\": " << summary.mean << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"min\": " << summary.min << ",\n";
  write_indent(output, indent_level + 1);
  output << "\"max\": " << summary.max << "\n";
  write_indent(output, indent_level);
  output << "}";
}

}  // namespace

double compute_events_per_second(const RunSummary& summary) {
  return summary.elapsed_seconds > 0.0
      ? static_cast<double>(summary.events_processed) / summary.elapsed_seconds
      : 0.0;
}

BenchmarkSummary summarize_benchmark_runs(
    const std::vector<BenchmarkRunResult>& runs,
    const BenchmarkSamples& aggregate_samples) {
  BenchmarkSummary summary;
  summary.repeat_count = runs.size();

  std::vector<double> total_events_seen;
  std::vector<double> events_processed;
  std::vector<double> elapsed_seconds;
  std::vector<double> events_per_second;
  std::vector<double> arbitrage_detections;
  std::vector<double> max_queue_depth;

  total_events_seen.reserve(runs.size());
  events_processed.reserve(runs.size());
  elapsed_seconds.reserve(runs.size());
  events_per_second.reserve(runs.size());
  arbitrage_detections.reserve(runs.size());
  max_queue_depth.reserve(runs.size());

  for (const auto& run : runs) {
    total_events_seen.push_back(static_cast<double>(run.summary.total_events_seen));
    events_processed.push_back(static_cast<double>(run.summary.events_processed));
    elapsed_seconds.push_back(run.summary.elapsed_seconds);
    events_per_second.push_back(run.events_per_second);
    arbitrage_detections.push_back(static_cast<double>(run.summary.arbitrage_detections));
    max_queue_depth.push_back(static_cast<double>(run.summary.max_queue_depth));
  }

  summary.total_events_seen = summarize_numeric_values(total_events_seen);
  summary.events_processed = summarize_numeric_values(events_processed);
  summary.elapsed_seconds = summarize_numeric_values(elapsed_seconds);
  summary.events_per_second = summarize_numeric_values(events_per_second);
  summary.arbitrage_detections = summarize_numeric_values(arbitrage_detections);
  summary.max_queue_depth = summarize_numeric_values(max_queue_depth);
  summary.logic_latency = summarize_latencies(aggregate_samples.logic_latency_ns);
  summary.stage_latencies = summarize_stage_latencies(aggregate_samples.stage_latencies);
  return summary;
}

bool write_benchmark_report_json(
    const std::string& output_path,
    const BenchmarkReportMetadata& metadata,
    const BenchmarkSummary& aggregate,
    const std::vector<BenchmarkRunResult>& runs,
    std::string& error_message) {
  std::filesystem::path path(output_path);
  if (!path.parent_path().empty()) {
    std::error_code mkdir_error;
    std::filesystem::create_directories(path.parent_path(), mkdir_error);
    if (mkdir_error) {
      error_message = "Could not create benchmark report directory: " + path.parent_path().string();
      return false;
    }
  }

  std::ofstream output(path);
  if (!output.is_open()) {
    error_message = "Could not open benchmark report path: " + output_path;
    return false;
  }

  output << std::fixed << std::setprecision(6);
  output << "{\n";
  write_indent(output, 1);
  output << "\"metadata\": {\n";
  write_indent(output, 2);
  output << "\"input_path\": \"" << escape_json(metadata.input_path) << "\",\n";
  write_indent(output, 2);
  output << "\"repeat_count\": " << metadata.repeat_count << ",\n";
  write_indent(output, 2);
  output << "\"warmup_events\": " << metadata.warmup_events << ",\n";
  write_indent(output, 2);
  output << "\"fee_bps\": " << metadata.fee_bps << ",\n";
  write_indent(output, 2);
  output << "\"symbol_count\": " << metadata.symbol_count << ",\n";
  write_indent(output, 2);
  output << "\"input_event_count\": " << metadata.input_event_count << "\n";
  write_indent(output, 1);
  output << "},\n";

  write_indent(output, 1);
  output << "\"aggregate\": {\n";
  write_indent(output, 2);
  output << "\"repeat_count\": " << aggregate.repeat_count << ",\n";
  write_indent(output, 2);
  output << "\"total_events_seen\": ";
  write_numeric_summary(output, aggregate.total_events_seen, 0);
  output << ",\n";
  write_indent(output, 2);
  output << "\"events_processed\": ";
  write_numeric_summary(output, aggregate.events_processed, 0);
  output << ",\n";
  write_indent(output, 2);
  output << "\"elapsed_seconds\": ";
  write_numeric_summary(output, aggregate.elapsed_seconds, 0);
  output << ",\n";
  write_indent(output, 2);
  output << "\"events_per_second\": ";
  write_numeric_summary(output, aggregate.events_per_second, 0);
  output << ",\n";
  write_indent(output, 2);
  output << "\"arbitrage_detections\": ";
  write_numeric_summary(output, aggregate.arbitrage_detections, 0);
  output << ",\n";
  write_indent(output, 2);
  output << "\"max_queue_depth\": ";
  write_numeric_summary(output, aggregate.max_queue_depth, 0);
  output << ",\n";
  write_indent(output, 2);
  output << "\"logic_latency\": ";
  write_latency_summary(output, aggregate.logic_latency, 0);
  output << ",\n";
  write_indent(output, 2);
  output << "\"stage_latencies\": ";
  write_stage_latency_summary(output, aggregate.stage_latencies, 0);
  output << "\n";
  write_indent(output, 1);
  output << "},\n";

  write_indent(output, 1);
  output << "\"runs\": [\n";
  for (std::size_t index = 0; index < runs.size(); ++index) {
    const auto& run = runs[index];
    write_indent(output, 2);
    output << "{\n";
    write_indent(output, 3);
    output << "\"run_index\": " << run.run_index << ",\n";
    write_indent(output, 3);
    output << "\"total_events_seen\": " << run.summary.total_events_seen << ",\n";
    write_indent(output, 3);
    output << "\"events_processed\": " << run.summary.events_processed << ",\n";
    write_indent(output, 3);
    output << "\"elapsed_seconds\": " << run.summary.elapsed_seconds << ",\n";
    write_indent(output, 3);
    output << "\"events_per_second\": " << run.events_per_second << ",\n";
    write_indent(output, 3);
    output << "\"arbitrage_detections\": " << run.summary.arbitrage_detections << ",\n";
    write_indent(output, 3);
    output << "\"max_queue_depth\": " << run.summary.max_queue_depth << ",\n";
    write_indent(output, 3);
    output << "\"logic_latency\": ";
    write_latency_summary(output, run.summary.logic_latency, 0);
    output << ",\n";
    write_indent(output, 3);
    output << "\"stage_latencies\": ";
    write_stage_latency_summary(output, run.summary.stage_latencies, 0);
    output << "\n";
    write_indent(output, 2);
    output << "}";
    if (index + 1 < runs.size()) {
      output << ",";
    }
    output << "\n";
  }
  write_indent(output, 1);
  output << "]\n";
  output << "}\n";

  return true;
}
