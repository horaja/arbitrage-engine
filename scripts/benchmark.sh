#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
RESULTS_DIR="${RESULTS_DIR:-benchmark_results}"
PLOTS_DIR="${PLOTS_DIR:-benchmark_plots}"
REPEAT="${REPEAT:-5}"
WARMUP_EVENTS="${WARMUP_EVENTS:-10000}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-RelWithDebInfo}"

profiles=("1m_3" "1m_30" "1m_300")

echo "== Phase 3 benchmark workflow =="
echo "build_dir=${BUILD_DIR}"
echo "results_dir=${RESULTS_DIR}"
echo "plots_dir=${PLOTS_DIR}"
echo "repeat=${REPEAT}"
echo "warmup_events=${WARMUP_EVENTS}"
echo "cmake_build_type=${CMAKE_BUILD_TYPE}"
echo

echo "== Configure + build =="
cmake -S cpp_engine -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
cmake --build "${BUILD_DIR}"
echo

echo "== Smoke tests =="
"./${BUILD_DIR}/smoke_tests"
echo

echo "== Generate deterministic benchmark data =="
for profile in "${profiles[@]}"; do
  python3 tools/generate_replay.py --profile "${profile}"
done
echo

mkdir -p "${RESULTS_DIR}"

echo "== Run benchmarks =="
for profile in "${profiles[@]}"; do
  input_path="benchmark_data/generated_${profile}.csv"
  output_path="${RESULTS_DIR}/${profile}.json"

  echo "-- ${profile} --"
  "./${BUILD_DIR}/arb_benchmark" \
    --input "${input_path}" \
    --repeat "${REPEAT}" \
    --warmup-events "${WARMUP_EVENTS}" \
    --output-json "${output_path}"
  echo
done

echo "== Plot benchmark reports =="
python3 tools/plot_benchmark.py "${RESULTS_DIR}" --output-dir "${PLOTS_DIR}"
echo

echo "Done."
echo "JSON reports: ${RESULTS_DIR}/"
echo "Plots: ${PLOTS_DIR}/"