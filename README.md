# Arbitrage Engine Prototype

This repository is a replay-driven triangular arbitrage prototype with a C++ hot path. The current codebase normalizes replay data into typed market events, feeds them through a small replay core, updates a graph-backed market model, and reports detected arbitrage cycles.

`implementation_plan.md` is the forward roadmap for the replay-first engine, benchmark work, and later risk-modeling extensions. Those later phases are not implemented yet.

## Current Scope

- Typed `MarketEvent` envelope for replayed trade ticks
- `ReplayAdapter` boundary with a CSV replay implementation
- Logical replay clock by default, with optional fixed wall-time delay
- CSV replay into a two-thread pipeline
- Lock-free SPSC queue between replay and strategy stages
- Graph-based arbitrage detection in C++
- Native smoke tests and a lightweight benchmark entrypoint

## Current Architecture

- `CsvReplayAdapter` reads deterministic fixture or logged CSV data and emits typed `MarketEvent` records.
- `ReplayRunner` consumes replay events through the adapter boundary and drives the existing two-thread pipeline.
- `Clock` controls replay timing. Benchmark mode uses deterministic logical replay; the main binary can also run with a fixed per-event delay via `--sleep-ms`.
- `ArbitrageGraph` remains the detection core for the current prototype.

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

Run the benchmark:

```bash
./build/arb_benchmark --input fixtures/sample_replay.csv
```

Run the smoke tests:

```bash
./build/smoke_tests
```

The smoke tests cover:

- adapter metadata and typed event parsing
- deterministic replay success and regression fixtures
- malformed CSV failure handling
- wall-time replay mode

## Optional Python Logger

`python_utils/data_logger.py` can collect live trade data into the same CSV shape used by the replay path.

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r python_utils/requirements.txt
python python_utils/data_logger.py
```
