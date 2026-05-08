#include "replay_runner.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "arbitragegraph.h"
#include "spsc_queue.h"

namespace {

struct ReplayMessage {
  enum class Kind {
    MarketEvent,
    EndOfStream
  };

  Kind kind = Kind::EndOfStream;
  MarketEvent event;

  static ReplayMessage from_event(MarketEvent market_event) {
    ReplayMessage message;
    message.kind = Kind::MarketEvent;
    message.event = std::move(market_event);
    return message;
  }

  static ReplayMessage end_of_stream() {
    return ReplayMessage{};
  }
};

std::string trim(const std::string& value) {
  const std::size_t start = value.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }

  const std::size_t end = value.find_last_not_of(" \t\r\n");
  return value.substr(start, end - start + 1);
}

bool read_next_event(std::istream& input_stream, MarketEvent& event, std::string& error_message) {
  std::string line;
  if (!std::getline(input_stream, line)) {
    return false;
  }

  std::stringstream row_stream(line);
  std::string timestamp_field;
  std::string symbol_field;
  std::string price_field;
  std::string quantity_field;

  if (!std::getline(row_stream, timestamp_field, ',') ||
      !std::getline(row_stream, symbol_field, ',') ||
      !std::getline(row_stream, price_field, ',') ||
      !std::getline(row_stream, quantity_field, ',')) {
    error_message = "Malformed CSV row: " + line;
    return false;
  }

  try {
    event.exchange_timestamp = trim(timestamp_field);
    event.symbol = trim(symbol_field);
    event.price = std::stod(trim(price_field));
    event.quantity = std::stod(trim(quantity_field));
  } catch (const std::exception& exception) {
    error_message = "Failed to parse CSV row: " + line + " (" + exception.what() + ")";
    return false;
  }

  if (event.symbol.empty()) {
    error_message = "Encountered an empty symbol in CSV row: " + line;
    return false;
  }

  return true;
}

LatencySummary summarize_latencies(std::vector<std::uint64_t> latencies_ns) {
  LatencySummary summary;
  if (latencies_ns.empty()) {
    return summary;
  }

  std::sort(latencies_ns.begin(), latencies_ns.end());

  const auto percentile_value = [&](double percentile) {
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(percentile * static_cast<double>(latencies_ns.size())) - 1.0);
    return latencies_ns[std::min(index, latencies_ns.size() - 1)];
  };

  std::uint64_t total_ns = 0;
  for (const std::uint64_t latency_ns : latencies_ns) {
    total_ns += latency_ns;
  }

  summary.min_ns = latencies_ns.front();
  summary.avg_ns = total_ns / latencies_ns.size();
  summary.p50_ns = percentile_value(0.50);
  summary.p95_ns = percentile_value(0.95);
  summary.p99_ns = percentile_value(0.99);
  summary.max_ns = latencies_ns.back();
  return summary;
}

void update_max_depth(std::atomic<std::size_t>& max_depth, std::size_t candidate_depth) {
  std::size_t observed = max_depth.load(std::memory_order_relaxed);
  while (candidate_depth > observed &&
         !max_depth.compare_exchange_weak(
             observed,
             candidate_depth,
             std::memory_order_relaxed,
             std::memory_order_relaxed)) {
  }
}

double calculate_profit_percent(const ArbitrageGraph& graph, const std::vector<std::string>& cycle) {
  double weight_sum = 0.0;
  for (std::size_t index = 0; index + 1 < cycle.size(); ++index) {
    weight_sum += graph.get_edge_weight(cycle[index], cycle[index + 1]);
  }

  const double profit_multiplier = std::exp(-weight_sum);
  return (profit_multiplier - 1.0) * 100.0;
}

void print_opportunity(const OpportunitySummary& opportunity) {
  std::cout << "\nArbitrage Opportunity\n";
  std::cout << "Cycle: ";
  for (std::size_t index = 0; index < opportunity.cycle.size(); ++index) {
    std::cout << opportunity.cycle[index];
    if (index + 1 < opportunity.cycle.size()) {
      std::cout << " -> ";
    }
  }
  std::cout << "\nProfit: " << opportunity.profit_percent << "%\n" << std::endl;
}

}  // namespace

ReplayRunner::ReplayRunner(EngineConfig config) : config_(std::move(config)) {}

std::optional<CsvReplaySource::Metadata> CsvReplaySource::inspect(
    const std::string& input_path,
    std::string& error_message) {
  std::ifstream input_stream(input_path);
  if (!input_stream.is_open()) {
    error_message = "Could not open input file: " + input_path;
    return std::nullopt;
  }

  std::string header_line;
  if (!std::getline(input_stream, header_line)) {
    error_message = "Input file is empty: " + input_path;
    return std::nullopt;
  }

  Metadata metadata;
  std::unordered_set<std::string> seen_symbols;
  MarketEvent event;

  while (true) {
    std::streampos row_start = input_stream.tellg();
    std::string parse_error;
    if (!read_next_event(input_stream, event, parse_error)) {
      if (input_stream.eof()) {
        break;
      }

      input_stream.clear();
      input_stream.seekg(row_start);
      std::string raw_row;
      std::getline(input_stream, raw_row);
      error_message = parse_error.empty() ? "Failed to parse CSV row: " + raw_row : parse_error;
      return std::nullopt;
    }

    ++metadata.event_count;
    if (seen_symbols.insert(event.symbol).second) {
      metadata.symbols.push_back(event.symbol);
    }
  }

  if (metadata.event_count == 0) {
    error_message = "Input file does not contain any replay rows: " + input_path;
    return std::nullopt;
  }

  return metadata;
}

RunSummary ReplayRunner::run() const {
  RunSummary summary;

  if (config_.input_path.empty()) {
    summary.error_message = "Missing required --input path.";
    return summary;
  }

  std::string validation_error;
  const std::optional<CsvReplaySource::Metadata> metadata =
      CsvReplaySource::inspect(config_.input_path, validation_error);
  if (!metadata) {
    summary.error_message = validation_error;
    return summary;
  }

  ArbitrageGraph graph(metadata->symbols);
  SPSCQueue<ReplayMessage> event_queue;

  std::atomic<std::size_t> max_queue_depth{0};
  std::atomic<bool> producer_failed{false};
  std::mutex error_mutex;
  std::string producer_error_message;
  std::vector<std::uint64_t> logic_latencies_ns;

  const auto start_time = std::chrono::steady_clock::now();

  std::thread replay_thread([&]() {
    std::ifstream input_stream(config_.input_path);
    if (!input_stream.is_open()) {
      {
        std::lock_guard<std::mutex> lock(error_mutex);
        producer_error_message = "Could not reopen input file during replay: " + config_.input_path;
      }
      producer_failed.store(true, std::memory_order_relaxed);
      event_queue.enqueue(ReplayMessage::end_of_stream());
      return;
    }

    std::string header_line;
    std::getline(input_stream, header_line);

    MarketEvent event;
    std::string parse_error;
    while (read_next_event(input_stream, event, parse_error)) {
      event_queue.enqueue(ReplayMessage::from_event(event));
      update_max_depth(max_queue_depth, event_queue.size_approx());

      if (config_.replay_sleep_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.replay_sleep_ms));
      }
    }

    if (!parse_error.empty()) {
      std::lock_guard<std::mutex> lock(error_mutex);
      producer_error_message = parse_error;
      producer_failed.store(true, std::memory_order_relaxed);
    }

    event_queue.enqueue(ReplayMessage::end_of_stream());
  });

  std::thread strategy_thread([&]() {
    while (true) {
      ReplayMessage message;
      event_queue.wait_dequeue(message);

      if (message.kind == ReplayMessage::Kind::EndOfStream) {
        break;
      }

      const auto logic_start = std::chrono::steady_clock::now();
      graph.update_price(message.event.symbol, message.event.price);
      const auto cycle = graph.find_arbitrage_cycle();
      const auto logic_end = std::chrono::steady_clock::now();

      logic_latencies_ns.push_back(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(logic_end - logic_start).count()));
      ++summary.events_processed;

      if (!cycle) {
        continue;
      }

      const double profit_percent = calculate_profit_percent(graph, cycle.value());
      if (profit_percent <= 0.0) {
        continue;
      }

      OpportunitySummary opportunity;
      opportunity.cycle = cycle.value();
      opportunity.profit_percent = profit_percent;

      ++summary.arbitrage_detections;
      summary.last_opportunity = opportunity;

      if (!config_.quiet) {
        print_opportunity(opportunity);
      }
    }
  });

  replay_thread.join();
  strategy_thread.join();

  const auto end_time = std::chrono::steady_clock::now();
  summary.elapsed_seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time).count();
  summary.max_queue_depth = max_queue_depth.load(std::memory_order_relaxed);
  summary.logic_latency = summarize_latencies(std::move(logic_latencies_ns));

  if (producer_failed.load(std::memory_order_relaxed)) {
    std::lock_guard<std::mutex> lock(error_mutex);
    summary.error_message = producer_error_message;
    return summary;
  }

  summary.succeeded = true;
  return summary;
}
