# Canonical Benchmark Result Schema

Every benchmark result file in `results/` follows this JSON schema regardless of
which tool produced it. Normalizer scripts in `tools/normalizers/` convert each
tool's native output into this format.

## File naming

```
{commit_short}_{tool}_{YYYYMMDD_HHMMSS}.json
```

Examples:
- `1fc34fb_arb_benchmark_20260607_143000.json`
- `1fc34fb_perf_stat_20260607_143100.json`
- `1fc34fb_cachegrind_20260607_143200.json`
- `1fc34fb_google_benchmark_20260607_143300.json`

## Schema

```json
{
  "schema_version": 1,
  "tool": "<tool name>",
  "commit": "<short commit hash>",
  "branch": "<branch name>",
  "timestamp": "<ISO 8601 UTC>",
  "machine": {
    "hostname": "<hostname>",
    "cpu_model": "<model string>",
    "cpu_cores": <int>,
    "os": "<OS description>",
    "kernel": "<kernel version or empty>"
  },
  "config": {
    "<tool-specific key>": "<value>"
  },
  "metrics": {
    "<metric_name>": {
      "value": <number>,
      "unit": "<unit string>"
    }
  },
  "tags": ["<optional user tags>"]
}
```

## Fields

| Field | Required | Description |
|---|---|---|
| `schema_version` | yes | Always `1` for this version. |
| `tool` | yes | One of: `arb_benchmark`, `perf_stat`, `cachegrind`, `google_benchmark`. |
| `commit` | yes | Short git commit hash at time of run. |
| `branch` | yes | Git branch name. |
| `timestamp` | yes | ISO 8601 UTC timestamp of the run. |
| `machine` | yes | Machine identification (auto-detected). |
| `config` | yes | Tool-specific configuration that affects results (input file, repeat count, flags, etc). |
| `metrics` | yes | Flat map of metric name to `{value, unit}` pairs. |
| `tags` | no | Optional list of user-supplied tags for filtering (e.g. `["baseline", "phase3"]`). |

## Unit conventions

| Unit | Meaning |
|---|---|
| `ns` | Nanoseconds |
| `us` | Microseconds |
| `ms` | Milliseconds |
| `s` | Seconds |
| `events/s` | Events per second |
| `count` | Dimensionless count |
| `ratio` | Dimensionless ratio |
| `percent` | Percentage (0-100) |
| `bytes` | Bytes |
| `refs` | Cache/memory references |

## Metric naming conventions

Use snake_case. Prefix with the stage or subsystem when applicable:

- `logic_latency_p50_ns`, `logic_latency_p99_ns`
- `stage_book_apply_quote_avg_ns`
- `throughput_events_per_second_mean`
- `cache_l1d_miss_rate`
- `instructions`, `cycles`, `ipc`
