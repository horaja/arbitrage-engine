#!/usr/bin/env bash
set -euo pipefail

# Unified benchmark runner.
# Runs a tool, captures its output, and normalizes into the canonical JSON format.
#
# Usage:
#   scripts/run_bench.sh arb_benchmark --input fixtures/sample_replay.csv --repeat 5
#   scripts/run_bench.sh perf_stat --input fixtures/sample_replay.csv
#   scripts/run_bench.sh cachegrind --input fixtures/sample_replay.csv
#   scripts/run_bench.sh all --input fixtures/sample_replay.csv
#
# Options:
#   --tags tag1,tag2    Attach tags to the result (e.g. "baseline,phase3")
#   --input <path>      Input CSV for the benchmark binary
#   --repeat <n>        Repeat count for arb_benchmark (default: 5)
#   --warmup <n>        Warmup events for arb_benchmark (default: 0)
#   --fee-bps <n>       Fee in basis points for arb_benchmark (default: 0)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build}"
NORMALIZER_DIR="${REPO_ROOT}/tools/normalizers"
RESULTS_DIR="${REPO_ROOT}/benchmarks/results"
TMP_DIR="${REPO_ROOT}/benchmarks/.tmp"

TOOL=""
INPUT=""
REPEAT=5
WARMUP=0
FEE_BPS=0
TAGS=""
PERF_EVENTS="cycles,instructions,cache-misses,cache-references,branch-misses,branch-instructions,L1-dcache-loads,L1-dcache-load-misses,task-clock,context-switches,page-faults"

usage() {
  echo "Usage: $0 <tool> [options]"
  echo ""
  echo "Tools: arb_benchmark, perf_stat, cachegrind, all"
  echo ""
  echo "Options:"
  echo "  --input <path>     Input CSV file (required)"
  echo "  --repeat <n>       Repeat count for arb_benchmark (default: 5)"
  echo "  --warmup <n>       Warmup events (default: 0)"
  echo "  --fee-bps <n>      Fee basis points (default: 0)"
  echo "  --tags <t1,t2>     Comma-separated tags"
  exit 1
}

if [[ $# -lt 1 ]]; then
  usage
fi

TOOL="$1"
shift

while [[ $# -gt 0 ]]; do
  case "$1" in
    --input)    INPUT="$2";    shift 2 ;;
    --repeat)   REPEAT="$2";   shift 2 ;;
    --warmup)   WARMUP="$2";   shift 2 ;;
    --fee-bps)  FEE_BPS="$2";  shift 2 ;;
    --tags)     TAGS="$2";     shift 2 ;;
    *)          echo "Unknown option: $1"; usage ;;
  esac
done

if [[ -z "${INPUT}" ]]; then
  echo "error: --input is required"
  usage
fi

mkdir -p "${RESULTS_DIR}" "${TMP_DIR}"

tag_args=()
if [[ -n "${TAGS}" ]]; then
  IFS=',' read -ra tag_list <<< "${TAGS}"
  for t in "${tag_list[@]}"; do
    tag_args+=(--tags "$t")
  done
fi

ensure_built() {
  if [[ ! -f "${BUILD_DIR}/arb_benchmark" ]]; then
    echo "== Building =="
    cmake -S "${REPO_ROOT}/cpp_engine" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "${BUILD_DIR}"
    echo
  fi
}

run_arb_benchmark() {
  ensure_built
  local raw_output="${TMP_DIR}/arb_benchmark_raw.json"

  echo "== arb_benchmark =="
  "${BUILD_DIR}/arb_benchmark" \
    --input "${INPUT}" \
    --repeat "${REPEAT}" \
    --warmup-events "${WARMUP}" \
    --output-json "${raw_output}"

  cd "${NORMALIZER_DIR}"
  python3 normalize_arb_benchmark.py "${raw_output}" "${tag_args[@]+"${tag_args[@]}"}"
  cd "${REPO_ROOT}"
  echo
}

run_perf_stat() {
  if ! command -v perf &>/dev/null; then
    echo "warning: perf not found, skipping perf_stat"
    return
  fi

  ensure_built
  local raw_output="${TMP_DIR}/perf_stat_raw.txt"
  local command_str="${BUILD_DIR}/arb_benchmark --input ${INPUT} --repeat 1 --warmup-events ${WARMUP}"

  echo "== perf stat =="
  perf stat -e "${PERF_EVENTS}" \
    ${command_str} \
    2>"${raw_output}" || true

  cd "${NORMALIZER_DIR}"
  python3 normalize_perf_stat.py "${raw_output}" --command "${command_str}" "${tag_args[@]+"${tag_args[@]}"}"
  cd "${REPO_ROOT}"
  echo
}

run_cachegrind() {
  if ! command -v valgrind &>/dev/null; then
    echo "warning: valgrind not found, skipping cachegrind"
    return
  fi

  ensure_built
  local cg_out="${TMP_DIR}/cachegrind.out"
  local annotated="${TMP_DIR}/cachegrind_annotated.txt"
  local command_str="${BUILD_DIR}/arb_benchmark --input ${INPUT} --repeat 1 --warmup-events ${WARMUP}"

  echo "== cachegrind =="
  valgrind --tool=cachegrind --cachegrind-out-file="${cg_out}" \
    ${command_str} \
    2>/dev/null || true

  cg_annotate "${cg_out}" > "${annotated}"

  cd "${NORMALIZER_DIR}"
  python3 normalize_cachegrind.py "${annotated}" --command "${command_str}" "${tag_args[@]+"${tag_args[@]}"}"
  cd "${REPO_ROOT}"
  echo
}

case "${TOOL}" in
  arb_benchmark)  run_arb_benchmark ;;
  perf_stat)      run_perf_stat ;;
  cachegrind)     run_cachegrind ;;
  all)
    run_arb_benchmark
    run_perf_stat
    run_cachegrind
    ;;
  *)
    echo "error: unknown tool '${TOOL}'"
    usage
    ;;
esac

echo "== Results =="
ls -lt "${RESULTS_DIR}"/*.json 2>/dev/null | head -5
echo
echo "Done. Results in: ${RESULTS_DIR}/"
