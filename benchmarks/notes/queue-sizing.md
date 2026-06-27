# Configurable SPSC capacity

## Change
The SPSC queue capacity is now an `EngineConfig` field (`queue_capacity`, default
4096) instead of the hardcoded `SPSCQueue` default. `arb_benchmark` exposes it via
`--queue-capacity <n>`. With `MarketEvent` now trivially copyable, the queued
`ReplayMessage` is trivially copyable too — enforced by a `static_assert` — so the
queue moves elements as flat POD copies with no per-element allocation.

## Hypothesis
The benchmark harness documented `max_queue_depth` saturating at 4096 under
throughput-heavy replays, i.e. the producer floods the queue and spins on
capacity. Making capacity tunable lets us explore the backpressure/throughput
trade-off (does a larger ring reduce producer spin and raise
`throughput_events_per_second_mean`, or just add memory and cache pressure?)
without recompiling.

## Confirming metric
- Sweep `--queue-capacity` and compare `throughput_events_per_second_mean`,
  `avg_queue_depth`, `p95_queue_depth`, and `max_queue_depth` across runs.
- `pipeline_backlog_latency` distribution as a function of capacity.

## Risks / notes
- Capacity is rounded up to a power of two by `SPSCQueue`.
- This note's hypothesis is the clearest candidate for early measurement once the
  benchmarking capability returns, since the saturation signal already exists.
- Possible follow-up if measurement justifies it: batched dequeue to amortize the
  per-event atomic/semaphore traffic.
