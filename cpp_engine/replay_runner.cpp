#include "replay_runner.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "arbitragegraph.h"
#include "book_builder.h"
#include "replay_adapter.h"
#include "spsc_queue.h"

namespace {

struct ReplayMessage {
  enum class Kind {
    MarketEvent,
    EndOfStream
  };

  Kind kind = Kind::EndOfStream;
  std::size_t sequence_number = 0;
  std::uint64_t adapter_next_event_ns = 0;
  std::chrono::steady_clock::time_point enqueue_time{};
  MarketEvent event;

  static ReplayMessage from_event(
      MarketEvent market_event,
      std::size_t sequence_number,
      std::uint64_t adapter_next_event_ns,
      std::chrono::steady_clock::time_point enqueue_time) {
    ReplayMessage message;
    message.kind = Kind::MarketEvent;
    message.sequence_number = sequence_number;
    message.adapter_next_event_ns = adapter_next_event_ns;
    message.enqueue_time = enqueue_time;
    message.event = std::move(market_event);
    return message;
  }

  static ReplayMessage end_of_stream() {
    return ReplayMessage{};
  }
};

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

struct LegInfo {
  double rate = 0.0;
  double source_max = 0.0;
};

bool build_leg_lookup(
    const BookBuilder& books,
    const std::vector<std::string>& symbols,
    std::map<std::pair<std::string, std::string>, LegInfo>& out) {
  out.clear();
  for (const auto& symbol : symbols) {
    const auto delimiter_pos = symbol.find('-');
    if (delimiter_pos == std::string::npos) {
      return false;
    }
    const std::string base = symbol.substr(0, delimiter_pos);
    const std::string quote = symbol.substr(delimiter_pos + 1);

    const TopOfBookQuote* latest = books.latest(symbol);
    if (latest == nullptr) {
      continue;
    }
    out[{base, quote}] = LegInfo{latest->bid_price, latest->bid_size};
    out[{quote, base}] = LegInfo{1.0 / latest->ask_price, latest->ask_size * latest->ask_price};
  }
  return true;
}

std::vector<std::string> rotate_cycle(const std::vector<std::string>& cycle) {
  if (cycle.size() < 2) {
    return cycle;
  }
  const std::size_t distinct = cycle.size() - 1;

  std::size_t anchor_index = 0;
  bool found_usd = false;
  for (std::size_t i = 0; i < distinct; ++i) {
    if (cycle[i] == "USD") {
      anchor_index = i;
      found_usd = true;
      break;
    }
  }

  if (!found_usd) {
    std::string canonical_anchor = cycle[0];
    for (std::size_t i = 1; i < distinct; ++i) {
      if (cycle[i] < canonical_anchor) {
        canonical_anchor = cycle[i];
        anchor_index = i;
      }
    }
  }

  std::vector<std::string> rotated;
  rotated.reserve(cycle.size());
  for (std::size_t k = 0; k < distinct; ++k) {
    rotated.push_back(cycle[(anchor_index + k) % distinct]);
  }
  rotated.push_back(rotated.front());
  return rotated;
}

bool compute_opportunity(
    const std::vector<std::string>& raw_cycle,
    const BookBuilder& books,
    const std::vector<std::string>& symbols,
    double fee_bps,
    OpportunitySummary& out) {
  if (raw_cycle.size() < 3) {
    return false;
  }

  std::map<std::pair<std::string, std::string>, LegInfo> leg_lookup;
  if (!build_leg_lookup(books, symbols, leg_lookup)) {
    return false;
  }

  const std::vector<std::string> cycle = rotate_cycle(raw_cycle);
  const double leg_fee_factor = 1.0 - fee_bps / 10000.0;

  double cumulative_source_per_anchor = 1.0;
  double gross_multiplier = 1.0;
  double net_multiplier = 1.0;
  double max_anchor = std::numeric_limits<double>::infinity();

  for (std::size_t i = 0; i + 1 < cycle.size(); ++i) {
    const auto iter = leg_lookup.find({cycle[i], cycle[i + 1]});
    if (iter == leg_lookup.end()) {
      return false;
    }
    const LegInfo& leg = iter->second;
    if (leg.source_max <= 0.0 || cumulative_source_per_anchor <= 0.0) {
      return false;
    }

    const double anchor_cap_for_leg = leg.source_max / cumulative_source_per_anchor;
    if (anchor_cap_for_leg < max_anchor) {
      max_anchor = anchor_cap_for_leg;
    }

    gross_multiplier *= leg.rate;
    net_multiplier *= leg.rate * leg_fee_factor;
    cumulative_source_per_anchor *= leg.rate;
  }

  out.cycle = cycle;
  out.gross_profit_percent = (gross_multiplier - 1.0) * 100.0;
  out.net_profit_percent = (net_multiplier - 1.0) * 100.0;
  out.max_executable_size = max_anchor;
  out.anchor_currency = cycle.front();
  return true;
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
  std::cout << "\nGross: " << opportunity.gross_profit_percent << "%"
            << "  Net: " << opportunity.net_profit_percent << "%"
            << "  MaxSize(" << opportunity.anchor_currency << "): "
            << opportunity.max_executable_size << "\n"
            << std::endl;
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

bool extract_top_of_book(
    const MarketEvent& event,
    TopOfBookQuote& quote,
    std::string& error_message) {
  if (event.event_type != MarketEventType::TopOfBookQuote || !event.top_of_book.has_value()) {
    error_message = "Unsupported market event payload for symbol: " + event.symbol;
    return false;
  }

  quote = event.top_of_book.value();
  return true;
}

bool is_measured_event(std::size_t sequence_number, std::size_t warmup_events) {
  return sequence_number > warmup_events;
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

  const std::vector<std::string> symbols = replay_adapter->metadata().symbols;
  ArbitrageGraph graph(symbols);
  BookBuilder books;
  SPSCQueue<ReplayMessage> event_queue;
  std::unique_ptr<Clock> replay_clock = make_clock(config_);

  std::atomic<std::size_t> max_queue_depth{0};
  std::atomic<bool> producer_failed{false};
  std::atomic<bool> consumer_failed{false};
  std::mutex error_mutex;
  std::string producer_error_message;
  std::string consumer_error_message;
  BenchmarkSamples benchmark_samples;
  std::optional<std::chrono::steady_clock::time_point> measured_start_time;

  std::thread replay_thread([&]() {
    std::optional<MarketEvent> previous_event;
    MarketEvent event;
    std::string parse_error;
    std::size_t sequence_number = 0;

    while (true) {
      const auto adapter_start = std::chrono::steady_clock::now();
      const bool has_event = replay_adapter->next_event(event, parse_error);
      const auto adapter_end = std::chrono::steady_clock::now();

      if (!has_event) {
        break;
      }

      replay_clock->before_event(previous_event, event);
      ++sequence_number;

      const std::uint64_t adapter_latency_ns = static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(adapter_end - adapter_start).count());
      const auto enqueue_time = std::chrono::steady_clock::now();

      event_queue.enqueue(ReplayMessage::from_event(
          event,
          sequence_number,
          adapter_latency_ns,
          enqueue_time));

      if (is_measured_event(sequence_number, config_.warmup_events)) {
        update_max_depth(max_queue_depth, event_queue.size_approx());
      }

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
      const auto dequeue_time = std::chrono::steady_clock::now();

      if (message.kind == ReplayMessage::Kind::EndOfStream) {
        break;
      }

      TopOfBookQuote quote;
      std::string payload_error;
      if (!extract_top_of_book(message.event, quote, payload_error)) {
        {
          std::lock_guard<std::mutex> lock(error_mutex);
          consumer_error_message = payload_error;
        }
        consumer_failed.store(true, std::memory_order_relaxed);
        continue;
      }

      ++summary.total_events_seen;
      const bool measured_event = is_measured_event(message.sequence_number, config_.warmup_events);

      if (measured_event) {
        ++summary.events_processed;
        benchmark_samples.stage_latencies.adapter_next_event_ns.push_back(message.adapter_next_event_ns);
        benchmark_samples.stage_latencies.queue_residence_latency_ns.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(dequeue_time - message.enqueue_time).count()));
      }

      const auto logic_start = std::chrono::steady_clock::now();
      if (measured_event && !measured_start_time.has_value()) {
        measured_start_time = logic_start;
      }

      const auto apply_start = logic_start;
      const bool quote_applied = books.apply_quote(message.event.symbol, quote);
      const auto apply_end = std::chrono::steady_clock::now();

      if (!measured_event) {
        if (!quote_applied) {
          continue;
        }

        graph.update_quote(message.event.symbol, quote);
        (void)graph.find_arbitrage_cycle();
        continue;
      }

      benchmark_samples.stage_latencies.book_apply_quote_ns.push_back(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(apply_end - apply_start).count()));

      if (!quote_applied) {
        benchmark_samples.logic_latency_ns.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(apply_end - logic_start).count()));
        continue;
      }

      const auto graph_start = std::chrono::steady_clock::now();
      graph.update_quote(message.event.symbol, quote);
      const auto graph_end = std::chrono::steady_clock::now();
      benchmark_samples.stage_latencies.graph_update_quote_ns.push_back(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(graph_end - graph_start).count()));

      const auto cycle_start = std::chrono::steady_clock::now();
      const auto cycle = graph.find_arbitrage_cycle();
      const auto cycle_end = std::chrono::steady_clock::now();
      benchmark_samples.stage_latencies.cycle_detection_ns.push_back(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(cycle_end - cycle_start).count()));

      if (cycle) {
        const auto opportunity_start = std::chrono::steady_clock::now();
        OpportunitySummary opportunity;
        const bool computed = compute_opportunity(
            cycle.value(),
            books,
            symbols,
            config_.fee_bps,
            opportunity);
        const auto opportunity_end = std::chrono::steady_clock::now();
        benchmark_samples.stage_latencies.opportunity_compute_ns.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(opportunity_end - opportunity_start).count()));

        if (computed && opportunity.gross_profit_percent > 0.0) {
          ++summary.arbitrage_detections;
          summary.last_opportunity = opportunity;

          if (!config_.quiet) {
            print_opportunity(opportunity);
          }
        }
      }

      const auto logic_end = std::chrono::steady_clock::now();
      benchmark_samples.logic_latency_ns.push_back(static_cast<std::uint64_t>(
          std::chrono::duration_cast<std::chrono::nanoseconds>(logic_end - logic_start).count()));
    }
  });

  replay_thread.join();
  strategy_thread.join();

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

  if (summary.total_events_seen <= config_.warmup_events) {
    summary.error_message =
        "Warmup consumed the entire replay: requested " +
        std::to_string(config_.warmup_events) +
        " warmup events, but replay only contained " +
        std::to_string(summary.total_events_seen) + ".";
    return summary;
  }

  const auto end_time = std::chrono::steady_clock::now();
  if (measured_start_time.has_value()) {
    summary.elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(
        end_time - measured_start_time.value()).count();
  }
  summary.max_queue_depth = max_queue_depth.load(std::memory_order_relaxed);
  summary.logic_latency = summarize_latencies(benchmark_samples.logic_latency_ns);
  summary.stage_latencies = summarize_stage_latencies(benchmark_samples.stage_latencies);
  summary.benchmark_samples = std::move(benchmark_samples);
  summary.succeeded = true;
  return summary;
}
