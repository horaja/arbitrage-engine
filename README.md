# Arbitrage Engine Prototype

This repository is a replay-driven triangular arbitrage prototype with a C++ hot path. The current codebase normalizes top-of-book quote data into typed market events, feeds them through a small replay core, builds per-symbol book state, and reports detected arbitrage cycles using executable bid/ask prices.

`implementation_plan.md` is the forward roadmap for the replay-first engine, benchmark work, and later risk-modeling extensions. Those later phases are not implemented yet.

## Current Scope

- Typed, trivially-copyable `MarketEvent` envelope carrying top-of-book quote payloads
- Integer-id symbol/currency interning (`SymbolRegistry`) built once at adapter open; only ids cross the hot path
- `ReplayAdapter` boundary with a CSV replay implementation
- `BookBuilder` maintains the latest valid top-of-book per symbol
- Logical replay clock by default, with optional fixed wall-time delay
- CSV replay into a two-thread pipeline
- Lock-free SPSC queue between replay and strategy stages
- Graph-based arbitrage detection driven by executable bid/ask prices
- Configurable global per-leg fee, with gross and net edge reporting
- Top-of-book size cap reported in the cycle's anchor currency
- Native smoke tests and an extensible benchmark entrypoint

## Current Architecture

- `CsvReplayAdapter` reads quote CSV files (`timestamp,symbol,bid_price,bid_size,ask_price,ask_size`) and emits typed `MarketEvent` records.
- `ReplayRunner` consumes replay events through the adapter boundary and drives the two-thread pipeline.
- Benchmark warmup is handled inside `ReplayRunner`, so benchmark runs can prime market state before the measured window begins.
- `BookBuilder` validates each quote (positive sides, non-crossed) and stores the most recent valid top-of-book per symbol; invalid quotes never overwrite the last known good state.
- `ArbitrageGraph` is updated from executable bid/ask: forward edge `BASE -> QUOTE` uses the bid; reverse edge `QUOTE -> BASE` uses `1/ask`. Invalid or unrelated symbols do not block evaluation for a valid triangle.
- Detected opportunities report `gross_profit_percent`, `net_profit_percent` (after applying `fee_bps` per leg multiplicatively), and `max_executable_size` in the anchor currency (USD when present in the cycle, otherwise the lexicographically earliest cycle currency).
- `Clock` controls replay timing. Benchmark mode uses deterministic logical replay; the main binary can also run with a fixed per-event delay via `--sleep-ms`.
- Benchmark metrics now include end-to-end measured logic latency plus stage timing for adapter reads, book updates, graph updates, cycle detection, opportunity computation, pipeline backlog latency, producer/consumer throughput, and producer-side queue-depth summaries.

## Build

Requirements:

- CMake 3.18+
- A C++17 compiler

From the repository root:

```bash
cmake -S cpp_engine -B build
cmake --build build
```

## Run

Run the prototype engine against the checked-in replay fixture:

```bash
./build/arb_engine --input fixtures/sample_replay.csv
```

Run with a fixed wall-time delay between replayed events:

```bash
./build/arb_engine --input fixtures/sample_replay.csv --sleep-ms 5
```

Run with a per-leg fee (basis points; `50` = 0.5% per leg):

```bash
./build/arb_engine --input fixtures/arb_fixture.csv --fee-bps 50
```

Run the benchmark:

```bash
./build/arb_benchmark --input fixtures/sample_replay.csv
```

Repeat the benchmark, skip a warmup prefix, and write structured JSON output:

```bash
./build/arb_benchmark \
  --input fixtures/sample_replay.csv \
  --repeat 5 \
  --warmup-events 2 \
  --output-json benchmark.json
```

Run the smoke tests:

```bash
./build/smoke_tests
```

The smoke tests cover:

- adapter metadata and typed quote-event parsing
- `BookBuilder` validity rules (positive sides, non-crossed, last-valid retention)
- deterministic replay success and regression fixtures
- warmup-window accounting inside `ReplayRunner`
- gross-vs-net behavior under a configured per-leg fee
- size-capped opportunity reporting
- malformed CSV failure handling
- wall-time replay mode

## Benchmark Tooling

Generate deterministic benchmark datasets in the untracked `benchmark_data/` directory:

```bash
python3 tools/generate_replay.py --profile 10k_3
python3 tools/generate_replay.py --profile 1m_30
```

Plot one or more benchmark JSON files:

```bash
python3 tools/plot_benchmark.py benchmark.json --output-dir benchmark_plots
```

`tools/plot_benchmark.py` requires `matplotlib`.

When plotting multiple JSON reports, the script creates one subdirectory per report for stage, backlog, logic, and queue-depth charts, and keeps `throughput_by_symbol_count.png` at the root output directory as the cross-report plot.

Current benchmark note:

- Queue-depth statistics are sampled on the producer side immediately after each measured enqueue.
- The SPSC queue capacity is configurable (`EngineConfig::queue_capacity`, default `4096`; `arb_benchmark --queue-capacity <n>`). `max_queue_depth` can saturate at the configured capacity in throughput-heavy runs, meaning the producer is flooding the queue and spinning on capacity — a useful stress signal and now a tunable knob (see [benchmarks/notes/queue-sizing.md](benchmarks/notes/queue-sizing.md)).
- The queue-depth-over-event-index series is downsampled for large runs so benchmark JSON stays usable on long replays.
