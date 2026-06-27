#include <filesystem>
#include <iostream>
#include <string>

#include "book_builder.h"
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
    // 2026-01-01T00:00:00Z == 1767225600 epoch seconds (~56 years after 1970).
    all_passed &= require(first_event.exchange_time_ns == 1767225600000000000LL, "first event exchange timestamp should parse to ns");
    all_passed &= require(first_event.receive_time_ns == first_event.exchange_time_ns, "first event receive timestamp should match exchange timestamp");
    all_passed &= require(first_event.event_type == MarketEventType::TopOfBookQuote, "first event should be a top-of-book quote");
    std::uint32_t btc_usd_id = 0;
    all_passed &= require(adapter.registry().find_symbol("BTC-USD", btc_usd_id), "registry should resolve BTC-USD");
    all_passed &= require(first_event.symbol_id == btc_usd_id, "first event symbol id should match registry");
    all_passed &= require(adapter.registry().symbol_name(first_event.symbol_id) == "BTC-USD", "symbol id should resolve back to BTC-USD");
    all_passed &= require(first_event.top_of_book.has_value(), "first event should carry a quote payload");
    all_passed &= require(first_event.top_of_book->bid_price == 50000.0, "first event bid should match");
    all_passed &= require(first_event.top_of_book->bid_size == 1.0, "first event bid size should match");
    all_passed &= require(first_event.top_of_book->ask_price == 50010.0, "first event ask should match");
    all_passed &= require(first_event.top_of_book->ask_size == 1.0, "first event ask size should match");
  }

  {
    SymbolRegistry registry;
    std::string intern_error;
    std::uint32_t btc_usd = 0;
    std::uint32_t eth_usd = 0;
    all_passed &= require(registry.intern_symbol("BTC-USD", btc_usd, intern_error), "registry should intern BTC-USD");
    all_passed &= require(registry.intern_symbol("ETH-USD", eth_usd, intern_error), "registry should intern ETH-USD");
    std::uint32_t btc_usd_again = 99;
    all_passed &= require(registry.intern_symbol("BTC-USD", btc_usd_again, intern_error), "re-interning should succeed");
    all_passed &= require(btc_usd_again == btc_usd, "re-interning a symbol should return the same id");
    all_passed &= require(registry.symbol_name(btc_usd) == "BTC-USD", "symbol id should round-trip to its name");

    BookBuilder books(registry.symbol_count());
    all_passed &= require(books.apply_quote(btc_usd, {50000, 1.0, 50010, 1.0}), "valid quote should be accepted");
    all_passed &= require(!books.apply_quote(btc_usd, {0.0, 1.0, 50010, 1.0}), "non-positive bid should be rejected");
    all_passed &= require(!books.apply_quote(btc_usd, {50020, 1.0, 50010, 1.0}), "crossed book should be rejected");
    all_passed &= require(books.latest(btc_usd) != nullptr, "last valid quote should remain after rejected updates");
    all_passed &= require(books.latest(btc_usd)->bid_price == 50000.0, "latest bid should reflect last valid quote");
    all_passed &= require(books.all_available({btc_usd}), "BookBuilder should report availability for known symbol");
    all_passed &= require(!books.all_available({btc_usd, eth_usd}), "BookBuilder should reject availability when a symbol is missing");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("sample_replay.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "sample replay should succeed");
    all_passed &= require(summary.total_events_seen == 6, "sample replay should see 6 total events");
    all_passed &= require(summary.events_processed == 6, "sample replay should process 6 events");
    all_passed &= require(summary.arbitrage_detections == 0, "sample replay has realistic spreads and should detect no arbitrage");
    all_passed &= require(summary.producer_elapsed_seconds > 0.0, "sample replay should record producer elapsed time");
    all_passed &= require(summary.consumer_elapsed_seconds > 0.0, "sample replay should record consumer elapsed time");
    all_passed &= require(summary.producer_events_per_second > 0.0, "sample replay should record producer throughput");
    all_passed &= require(summary.consumer_events_per_second > 0.0, "sample replay should record consumer throughput");
    all_passed &= require(summary.benchmark_samples.logic_latency_ns.size() == 6, "sample replay should record logic latency for every measured event");
    all_passed &= require(summary.logic_latency.sample_count == 6, "sample replay logic latency sample count should match measured events");
    all_passed &= require(summary.stage_latencies.adapter_next_event.sample_count == 6, "sample replay adapter sample count should match measured events");
    all_passed &= require(summary.benchmark_samples.stage_latencies.adapter_next_event_ns.size() == 6, "sample replay should record adapter timing for every measured event");
    all_passed &= require(summary.benchmark_samples.stage_latencies.pipeline_backlog_latency_ns.size() == 6, "sample replay should record pipeline backlog latency for every measured event");
    all_passed &= require(!summary.benchmark_samples.queue_depth_samples.empty(), "sample replay should record queue depth samples");
    all_passed &= require(summary.queue_depth.p95_depth <= summary.max_queue_depth, "sample replay p95 queue depth should not exceed max depth");
    all_passed &= require(summary.queue_depth.avg_depth <= static_cast<double>(summary.max_queue_depth), "sample replay avg queue depth should not exceed max depth");
    all_passed &= require(!summary.benchmark_samples.queue_depth_series.empty(), "sample replay should record queue depth series points");
    all_passed &= require(summary.benchmark_samples.queue_depth_series.back().event_index == summary.events_processed, "queue depth series should include the final measured event");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("sample_replay.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;
    config.warmup_events = 2;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "warmup replay should succeed");
    all_passed &= require(summary.total_events_seen == 6, "warmup replay should still see the full replay");
    all_passed &= require(summary.events_processed == 4, "warmup replay should only measure post-warmup events");
    all_passed &= require(summary.logic_latency.sample_count == 4, "warmup replay logic sample count should exclude warmup");
    all_passed &= require(summary.benchmark_samples.logic_latency_ns.size() == 4, "warmup replay should only retain measured logic samples");
    all_passed &= require(summary.benchmark_samples.stage_latencies.adapter_next_event_ns.size() == 4, "warmup replay should only retain measured adapter samples");
    all_passed &= require(!summary.benchmark_samples.queue_depth_series.empty(), "warmup replay should still record queue depth series points");
    all_passed &= require(summary.benchmark_samples.queue_depth_series.front().event_index >= 1, "queue depth series should use measured event indexes");
    all_passed &= require(summary.benchmark_samples.queue_depth_series.back().event_index == summary.events_processed, "warmup replay queue depth series should end at the measured event count");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("arb_fixture.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "arb fixture should succeed");
    all_passed &= require(summary.arbitrage_detections >= 1, "arb fixture should detect arbitrage");
    all_passed &= require(summary.last_opportunity.has_value(), "arb fixture should expose an opportunity");
    all_passed &= require(!summary.benchmark_samples.stage_latencies.opportunity_compute_ns.empty(), "arb fixture should record opportunity compute samples");
    all_passed &= require(summary.stage_latencies.opportunity_compute.sample_count > 0, "arb fixture opportunity compute sample count should be positive");
    if (summary.last_opportunity.has_value()) {
      const auto& opportunity = summary.last_opportunity.value();
      all_passed &= require(opportunity.gross_profit_percent > 0.0, "arb fixture gross profit should be positive");
      all_passed &= require(opportunity.net_profit_percent > 0.0, "arb fixture net profit at zero fees should be positive");
      all_passed &= require(opportunity.anchor_currency == "USD", "arb fixture cycle should anchor in USD");
      all_passed &= require(opportunity.max_executable_size > 0.0, "arb fixture should report a positive executable size");
    }
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("no_arb_fixture.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "no-arb fixture should succeed");
    all_passed &= require(summary.arbitrage_detections == 0, "no-arb fixture should not detect arbitrage");
    all_passed &= require(summary.stage_latencies.opportunity_compute.sample_count == 0, "no-arb fixture should not record opportunity compute samples");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("fee_eats_edge_fixture.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;
    config.fee_bps = 50.0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "fee fixture should succeed");
    all_passed &= require(summary.arbitrage_detections >= 1, "fee fixture should still detect a gross opportunity");
    if (summary.last_opportunity.has_value()) {
      const auto& opportunity = summary.last_opportunity.value();
      all_passed &= require(opportunity.gross_profit_percent > 0.0, "fee fixture gross profit should be positive");
      all_passed &= require(opportunity.net_profit_percent <= 0.0, "fee fixture net profit should be non-positive after fees");
    }
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("extra_symbol_invalid_fixture.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "invalid extra symbol fixture should succeed");
    all_passed &= require(summary.arbitrage_detections >= 1, "invalid extra symbol should not block triangle detection");
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("non_usd_anchor_fixture.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "non-USD anchor fixture should succeed");
    all_passed &= require(summary.arbitrage_detections >= 1, "non-USD anchor fixture should detect arbitrage");
    if (summary.last_opportunity.has_value()) {
      const auto& opportunity = summary.last_opportunity.value();
      all_passed &= require(opportunity.anchor_currency == "BTC", "non-USD cycles should anchor lexicographically");
    }
  }

  {
    EngineConfig config;
    config.input_path = fixture_path("size_limited_fixture.csv").string();
    config.quiet = true;
    config.replay_delay_ms = 0;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(summary.succeeded, "size-limited fixture should succeed");
    all_passed &= require(summary.arbitrage_detections >= 1, "size-limited fixture should detect arbitrage");
    if (summary.last_opportunity.has_value()) {
      const auto& opportunity = summary.last_opportunity.value();
      all_passed &= require(opportunity.anchor_currency == "USD", "size-limited fixture should anchor in USD");
      all_passed &= require(opportunity.max_executable_size > 0.0, "size-limited fixture should report a positive size");
      all_passed &= require(opportunity.max_executable_size < 10.0, "size-limited fixture's tiny ETH-BTC size should cap executable size");
    }
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

  {
    EngineConfig config;
    config.input_path = fixture_path("sample_replay.csv").string();
    config.quiet = true;
    config.warmup_events = 6;

    const RunSummary summary = ReplayRunner(config).run();
    all_passed &= require(!summary.succeeded, "warmup should fail when it consumes the entire replay");
    all_passed &= require(!summary.error_message.empty(), "warmup failure should return an error message");
  }

  if (!all_passed) {
    return 1;
  }

  std::cout << "All smoke tests passed.\n";
  return 0;
}
