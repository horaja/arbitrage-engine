# Array-indexed top-of-book state

## Change
`BookBuilder` stores the latest valid quote in a `std::vector<TopOfBookQuote>`
plus a `std::vector<bool> has_quote_`, both sized from `registry.symbol_count()`
at construction and indexed by `symbol_id`, replacing
`std::unordered_map<std::string, TopOfBookQuote>`. `apply_quote` and `latest`
are O(1) array access with no hashing.

## Hypothesis
Every event applied a quote, which previously hashed the symbol string and
indexed an `unordered_map` (with occasional rehash/reallocation as symbols were
first seen). With a dense id space known up front, a flat vector removes the hash
and guarantees no steady-state allocation, while improving locality (quotes for
nearby ids sit contiguously).

## Confirming metric
- `stage_book_apply_quote_avg_ns` and `bench_book_apply_quote` should drop to a
  handful of nanoseconds.
- No allocations attributable to `BookBuilder` after construction (profiler).

## Risks / notes
- `BookBuilder` now requires the symbol count at construction. All call sites
  (runner, smoke tests, microbench) construct it from the registry.
- `latest()` bounds-checks the id and returns `nullptr` for an un-quoted symbol,
  preserving the previous "missing symbol" contract used by `all_available` and
  the opportunity computation.
