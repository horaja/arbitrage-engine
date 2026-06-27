# Symbol/currency interning at the adapter

## Change
Trading-pair symbols and currency names are interned to dense integer ids once,
during `CsvReplayAdapter::open()`, in a new `SymbolRegistry`
(`cpp_engine/symbol_registry.h/.cpp`). `MarketEvent` now carries a
`std::uint32_t symbol_id` (and `int64_t` nanosecond timestamps) instead of three
`std::string` fields, making it trivially copyable. The CSV parse path was
rewritten to scan columns with `std::string_view` and parse numbers with
`strtod` over a stack buffer, removing per-row `stringstream`/`substr`/`trim`
heap churn. Downstream stages (`BookBuilder`, `ArbitrageGraph`) consume
`symbol_id` directly; currency *names* are resolved only on the rare
opportunity-reporting path.

## Hypothesis
The symbol string was the dominant per-event cost: it was allocated at parse,
copied into the event, carried through the SPSC queue, then hashed as a map key
in `BookBuilder` and re-parsed (`substr`) plus hashed twice in
`ArbitrageGraph::update_quote` — 4–6 string touches per event. Replacing it with
an integer id removes all steady-state symbol allocation and hashing, and makes
the queue payload a flat POD copy.

## Confirming metric
- `stage_book_apply_quote_avg_ns` and `stage_graph_update_quote_avg_ns` (and the
  `bench_symbol_resolve` / `bench_book_apply_quote` / `bench_graph_update_quote`
  microbenchmarks) should drop substantially.
- Steady-state heap allocation count on the parse→detect path should fall toward
  zero (verify with a profiler / allocation counter once available).
- `logic_latency_p50_ns` / `p99` should improve.

## Risks / notes
- Currency ids are assigned in first-seen order (previously sorted via
  `std::set`). This can change *which* of several simultaneous negative cycles is
  reported, but not whether an arbitrage exists — acceptable under the
  "semantically equivalent" contract. The single-triangle `arb_fixture` output is
  bit-identical before/after.
- Timestamps are parsed to epoch-ns (ISO 8601 `YYYY-MM-DDTHH:MM:SSZ` or bare
  numeric). They are passenger data only (the clock ignores them).
