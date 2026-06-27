// Google Benchmark microbenchmark scaffolding for the arbitrage-engine hot path.
//
// This target is built only when -DARB_BUILD_MICROBENCH=ON. It is intentionally
// scaffolding: the cases compile and run, but the numbers have not yet been
// tuned or acted on. Output is designed to flow through the existing canonical
// pipeline:
//
//   ./build/arb_microbench --benchmark_format=json --benchmark_out=gbench.json
//   python3 tools/normalizers/normalize_google_benchmark.py gbench.json
//
// Cases mirror the stages already tracked in benchmark_metrics.h so the
// per-stage microbench numbers line up with the end-to-end arb_benchmark report.

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include <benchmark/benchmark.h>

#include "arbitragegraph.h"
#include "book_builder.h"
#include "market_event.h"
#include "replay_adapter.h"
#include "spsc_queue.h"
#include "symbol_registry.h"

namespace {

constexpr const char* kSymbols[] = {"BTC-USD", "ETH-USD", "ETH-BTC"};

SymbolRegistry make_registry() {
  SymbolRegistry registry;
  std::string error;
  std::uint32_t id = 0;
  for (const char* symbol : kSymbols) {
    registry.intern_symbol(symbol, id, error);
  }
  return registry;
}

TopOfBookQuote sample_quote() {
  return TopOfBookQuote{50000.0, 1.0, 50010.0, 1.0};
}

// Writes a small replay CSV to a temp path and returns it (scaffolding fixture).
std::string write_fixture() {
  const std::string path =
      (std::filesystem::temp_directory_path() / "arb_microbench_fixture.csv").string();
  std::ofstream out(path);
  out << "timestamp,symbol,bid_price,bid_size,ask_price,ask_size\n";
  for (int i = 0; i < 1000; ++i) {
    for (const char* symbol : kSymbols) {
      out << "2026-01-01T00:00:00Z," << symbol << ",50000,1,50010,1\n";
    }
  }
  return path;
}

void bench_csv_parse(benchmark::State& state) {
  const std::string path = write_fixture();
  CsvReplayAdapter adapter(path);
  std::string error;
  adapter.open(error);
  MarketEvent event;
  for (auto _ : state) {
    if (!adapter.next_event(event, error)) {
      adapter.open(error);  // rewind at end of stream
      adapter.next_event(event, error);
    }
    benchmark::DoNotOptimize(event.symbol_id);
  }
  std::remove(path.c_str());
}
BENCHMARK(bench_csv_parse);

void bench_symbol_resolve(benchmark::State& state) {
  const SymbolRegistry registry = make_registry();
  const std::string symbol = "ETH-BTC";
  std::uint32_t id = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(registry.find_symbol(symbol, id));
  }
}
BENCHMARK(bench_symbol_resolve);

void bench_book_apply_quote(benchmark::State& state) {
  const SymbolRegistry registry = make_registry();
  BookBuilder books(registry.symbol_count());
  const TopOfBookQuote quote = sample_quote();
  for (auto _ : state) {
    benchmark::DoNotOptimize(books.apply_quote(0, quote));
  }
}
BENCHMARK(bench_book_apply_quote);

void bench_graph_update_quote(benchmark::State& state) {
  const SymbolRegistry registry = make_registry();
  ArbitrageGraph graph(registry);
  const TopOfBookQuote quote = sample_quote();
  for (auto _ : state) {
    graph.update_quote(0, quote);
    benchmark::ClobberMemory();
  }
}
BENCHMARK(bench_graph_update_quote);

void bench_cycle_detection(benchmark::State& state) {
  const SymbolRegistry registry = make_registry();
  ArbitrageGraph graph(registry);
  const TopOfBookQuote quote = sample_quote();
  for (std::uint32_t s = 0; s < registry.symbol_count(); ++s) {
    graph.update_quote(s, quote);
  }
  for (auto _ : state) {
    graph.update_quote(0, quote);
    auto cycle = graph.find_arbitrage_cycle();
    benchmark::DoNotOptimize(cycle);
  }
}
BENCHMARK(bench_cycle_detection);

void bench_queue_enqueue_dequeue(benchmark::State& state) {
  SPSCQueue<MarketEvent> queue(4096);
  MarketEvent event;
  event.top_of_book = sample_quote();
  MarketEvent out;
  for (auto _ : state) {
    queue.enqueue(event);
    queue.wait_dequeue(out);
    benchmark::DoNotOptimize(out.symbol_id);
  }
}
BENCHMARK(bench_queue_enqueue_dequeue);

}  // namespace

BENCHMARK_MAIN();
