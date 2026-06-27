# Producer/consumer ownership boundary

## Change
No code change in this note beyond what the interning work enabled; this
documents the concurrency boundary the engine now relies on.

The pipeline has exactly two threads joined by one SPSC queue:

- **Producer (`replay_thread`)** owns: reading CSV lines, parsing, timestamp
  parsing, and **symbol interning / id resolution**. It enqueues flat,
  trivially-copyable `ReplayMessage` records.
- **Consumer (`strategy_thread`)** owns: `BookBuilder` state, `ArbitrageGraph`
  updates and SPFA detection, and opportunity computation/reporting.

Because interning happens entirely on the producer side, **only integer ids cross
the queue** — no strings, no shared mutable string tables, no locks on the hot
path. The `SymbolRegistry` is built before either thread starts and is read-only
during replay, so both threads can reference it without synchronization.

## Hypothesis
A genuinely single-producer/single-consumer boundary lets us keep the lock-free
SPSC queue (with its `alignas(64)` head/tail false-sharing guard) rather than a
heavier MPMC queue, and keeps all symbol-table mutation off the hot path and out
of any critical section.

## Confirming metric
- The boundary is single-producer/single-consumer by construction (one enqueuer,
  one dequeuer). The relevant performance signal is queue behavior — see
  [queue-sizing.md](queue-sizing.md).
- Watch for false sharing between threads (cachegrind / `perf c2c`) on the queue
  head/tail and on the `RunSummary`/`BenchmarkSamples` written by the consumer.

## Risks / notes
- The error-reporting path still takes a mutex, but only on failure (off the hot
  path).
- If a future stage (e.g. execution simulation in Phase 4) adds a third owner,
  re-evaluate the topology before assuming SPSC still fits.
