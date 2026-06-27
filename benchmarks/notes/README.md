# Phase 3 Design Notes

These notes document the baseline Phase 3 systems optimizations. They are
**hypothesis-driven**: the changes were made on first-principles reasoning about
the hot path, not yet validated by measurement (benchmarking/profiling capability
was unavailable when they landed). Each note states the change, the hypothesized
benefit, and the metric that would confirm it once the harness is run.

To validate a hypothesis, capture before/after canonical results
(`benchmarks/results/`, schema in `benchmarks/SCHEMA.md`) using `arb_benchmark`
and the optional `arb_microbench` target:

```bash
cmake -S cpp_engine -B build -DARB_BUILD_MICROBENCH=ON
cmake --build build
./build/arb_microbench --benchmark_format=json --benchmark_out=gbench.json
python3 tools/normalizers/normalize_google_benchmark.py gbench.json
```

| Note | Optimization |
|---|---|
| [symbol-interning.md](symbol-interning.md) | Integer-id interning at the adapter |
| [csr-arbitrage-graph.md](csr-arbitrage-graph.md) | CSR adjacency + direct edge slots |
| [id-indexed-book-builder.md](id-indexed-book-builder.md) | Array-indexed top-of-book state |
| [queue-sizing.md](queue-sizing.md) | Configurable SPSC capacity |
| [concurrency-boundary.md](concurrency-boundary.md) | Producer/consumer ownership |
