# CSR adjacency + direct edge slots

## Change
`ArbitrageGraph` is now constructed from the `SymbolRegistry` and builds its full
topology once. Adjacency is stored in compressed-sparse-row (CSR) form: a flat
`std::vector<Edge> edges_` plus a per-vertex `row_start_` offset array, replacing
`std::vector<std::vector<Edge>>` and the `std::unordered_map<uint64_t,size_t>
edge_index_map`. Each symbol owns fixed forward/reverse edge slots
(`symbol_edge_slots_` indexed by `symbol_id`), so `update_quote(symbol_id, quote)`
is two direct array writes plus two dirty-vertex pushes — no string parsing, no
hashing, no `find`/`push_back`. SPFA working buffers (`distance`, `predecessor`,
`update_counts`, `in_queue`, and a reused FIFO `spfa_queue_`) are pre-sized at
construction; the per-detection `std::deque` copy is gone. Cycle reconstruction
was changed from two `insert(begin, …)` calls (O(n²)) to `push_back` +
`std::reverse` (O(n)).

## Hypothesis
The previous hot path did, per quote: 2 `substr` allocations, 2 currency-name
hash lookups, and 2 edge-key hash lookups, with potential vector reallocation on
first sight of an edge. CSR makes edge access contiguous and cache-friendly, and
direct slots reduce the update to pointer-free array writes. Reusing SPFA buffers
removes a per-call `std::deque` allocation. The reconstruction fix removes
quadratic behavior on long cycles.

## Confirming metric
- `stage_graph_update_quote_avg_ns` and `bench_graph_update_quote` should drop.
- `stage_cycle_detection_*_ns` and `bench_cycle_detection` should improve,
  especially p99 and on larger currency graphs.
- Cache-miss rate on the detection stage (cachegrind / `perf stat`
  `cache_l1d_miss_rate`) should fall versus the nested-vector layout.

## Risks / notes
- Un-quoted edges are initialized to `+infinity` so they never relax — this
  reproduces the old behavior where an edge did not exist until its first quote.
  Initializing to `0.0` instead introduced spurious cycles (caught by the no-arb
  smoke test).
- `get_edge_weight` (a debug accessor, not on the hot path) now does a linear CSR
  row scan; acceptable as it is not used in steady state.
