# Arbitrage Engine Prototype

This repository is a replay-driven triangular arbitrage prototype with a C++ hot path. The current codebase replays CSV market data, updates a graph-backed market model, and reports detected arbitrage cycles.

`implementation_plan.md` is the forward roadmap for the replay-first engine, benchmark work, and later risk-modeling extensions. Those later phases are not implemented yet.

## Current Scope

- CSV replay into a two-thread pipeline
- Lock-free SPSC queue between replay and strategy stages
- Graph-based arbitrage detection in C++
- Native smoke tests and a lightweight benchmark entrypoint

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

Run the benchmark:

```bash
./build/arb_benchmark --input fixtures/sample_replay.csv
```

Run the smoke tests:

```bash
./build/smoke_tests
```

## Optional Python Logger

`python_utils/data_logger.py` can collect live trade data into the same CSV shape used by the replay path.

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r python_utils/requirements.txt
python python_utils/data_logger.py
```
