#include "replay_runner.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "arbitragegraph.h"
#include "replay_adapter.h"
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

std::unique_ptr<Clock> make_clock(const EngineConfig& config) {
  if (config.clock_mode == ClockMode::WallTime && config.replay_delay_ms > 0) {
    return std::make_unique<FixedDelayClock>(std::chrono::milliseconds(config.replay_delay_ms));
  }

  return std::make_unique<LogicalReplayClock>();
}

std::unique_ptr<ReplayAdapter> make_replay_adapter(const EngineConfig& config) {
  return std::make_unique<CsvReplayAdapter>(config.input_path);
}

bool extract_trade_tick(
    const MarketEvent& event,
    TradeTick& trade_tick,
    std::string& error_message) {
  if (event.event_type != MarketEventType::TradeTick || !event.trade_tick.has_value()) {
    error_message = "Unsupported market event payload for symbol: " + event.symbol;
    return false;
  }

  trade_tick = event.trade_tick.value();
  return true;
}

}  // namespace

ReplayRunner::ReplayRunner(EngineConfig config) : config_(std::move(config)) {}

RunSummary ReplayRunner::run() const {
  RunSummary summary;

  if (config_.input_path.empty()) {
    summary.error_message = "Missing required --input path.";
    return summary;
  }

  std::string adapter_error;
  std::unique_ptr<ReplayAdapter> replay_adapter = make_replay_adapter(config_);
  if (!replay_adapter->open(adapter_error)) {
    summary.error_message = adapter_error;
    return summary;
  }

  ArbitrageGraph graph(replay_adapter->metadata().symbols);
  SPSCQueue<ReplayMessage> event_queue;
  std::unique_ptr<Clock> replay_clock = make_clock(config_);

  std::atomic<std::size_t> max_queue_depth{0};
  std::atomic<bool> producer_failed{false};
  std::atomic<bool> consumer_failed{false};
  std::mutex error_mutex;
  std::string producer_error_message;
  std::string consumer_error_message;
  std::vector<std::uint64_t> logic_latencies_ns;

  const auto start_time = std::chrono::steady_clock::now();

  std::thread replay_thread([&]() {
    std::optional<MarketEvent> previous_event;
    MarketEvent event;
    std::string parse_error;
    while (replay_adapter->next_event(event, parse_error)) {
      replay_clock->before_event(previous_event, event);
      event_queue.enqueue(ReplayMessage::from_event(event));
      update_max_depth(max_queue_depth, event_queue.size_approx());
      previous_event = event;
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

      TradeTick trade_tick;
      std::string payload_error;
      if (!extract_trade_tick(message.event, trade_tick, payload_error)) {
        {
          std::lock_guard<std::mutex> lock(error_mutex);
          consumer_error_message = payload_error;
        }
        consumer_failed.store(true, std::memory_order_relaxed);
        continue;
      }

      const auto logic_start = std::chrono::steady_clock::now();
      graph.update_price(message.event.symbol, trade_tick.price);
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

  if (consumer_failed.load(std::memory_order_relaxed)) {
    std::lock_guard<std::mutex> lock(error_mutex);
    summary.error_message = consumer_error_message;
    return summary;
  }

  summary.succeeded = true;
  return summary;
}
