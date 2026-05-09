# Arbitrage Engine Prototype

This repository is a replay-driven triangular arbitrage prototype with a C++ hot path. The current codebase normalizes top-of-book quote data into typed market events, feeds them through a small replay core, builds per-symbol book state, and reports detected arbitrage cycles using executable bid/ask prices.

`implementation_plan.md` is the forward roadmap for the replay-first engine, benchmark work, and later risk-modeling extensions. Those later phases are not implemented yet.

## Current Scope

- Typed `MarketEvent` envelope carrying top-of-book quote payloads
- `ReplayAdapter` boundary with a CSV replay implementation
- `BookBuilder` maintains the latest valid top-of-book per symbol
- Logical replay clock by default, with optional fixed wall-time delay
- CSV replay into a two-thread pipeline
- Lock-free SPSC queue between replay and strategy stages
- Graph-based arbitrage detection driven by executable bid/ask prices
- Configurable global per-leg fee, with gross and net edge reporting
- Top-of-book size cap reported in the cycle's anchor currency
- Native smoke tests and a lightweight benchmark entrypoint

## Current Architecture

- `CsvReplayAdapter` reads quote CSV files (`timestamp,symbol,bid_price,bid_size,ask_price,ask_size`) and emits typed `MarketEvent` records.
- `ReplayRunner` consumes replay events through the adapter boundary and drives the two-thread pipeline.
- `BookBuilder` validates each quote (positive sides, non-crossed) and stores the most recent valid top-of-book per symbol; invalid quotes never overwrite the last known good state.
- `ArbitrageGraph` is updated from executable bid/ask: forward edge `BASE -> QUOTE` uses the bid; reverse edge `QUOTE -> BASE` uses `1/ask`. Cycles are only evaluated once every tracked symbol has at least one valid quote.
- Detected opportunities report `gross_profit_percent`, `net_profit_percent` (after applying `fee_bps` per leg multiplicatively), and `max_executable_size` in the anchor currency (USD when present in the cycle, otherwise the first cycle vertex).
- `Clock` controls replay timing. Benchmark mode uses deterministic logical replay; the main binary can also run with a fixed per-event delay via `--sleep-ms`.

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

Run the smoke tests:

```bash
./build/smoke_tests
```

The smoke tests cover:

- adapter metadata and typed quote-event parsing
- `BookBuilder` validity rules (positive sides, non-crossed, last-valid retention)
- deterministic replay success and regression fixtures
- gross-vs-net behavior under a configured per-leg fee
- size-capped opportunity reporting
- malformed CSV failure handling
- wall-time replay mode

## Optional Python Logger

`python_utils/data_logger.py` can collect live market data. Note that the C++ replay path now expects quote-row CSVs (`timestamp,symbol,bid_price,bid_size,ask_price,ask_size`); update the logger before feeding its output back into the engine.

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r python_utils/requirements.txt
python python_utils/data_logger.py
```
