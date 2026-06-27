cmake_minimum_required(VERSION 3.18)

project(ArbitrageEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(arb_core
  arbitragegraph.cpp
  benchmark_metrics.cpp
  book_builder.cpp
  clock.cpp
  replay_adapter.cpp
  replay_runner.cpp
  symbol_registry.cpp
)

target_include_directories(arb_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/libs
)

add_executable(arb_engine engine_main.cpp)
target_link_libraries(arb_engine PRIVATE arb_core)

add_executable(arb_benchmark
  benchmark_main.cpp
  benchmark_report.cpp
)
target_link_libraries(arb_benchmark PRIVATE arb_core)

add_executable(smoke_tests smoke_tests.cpp)
target_compile_definitions(smoke_tests PRIVATE ARB_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(smoke_tests PRIVATE arb_core)

enable_testing()
add_test(
  NAME smoke_tests
  COMMAND smoke_tests
)

# Optional Google Benchmark microbenchmark target. Off by default so the normal
# build stays self-contained and offline-friendly; enable with
# -DARB_BUILD_MICROBENCH=ON to fetch Google Benchmark and build arb_microbench.
option(ARB_BUILD_MICROBENCH "Build the Google Benchmark microbenchmark suite" OFF)

if(ARB_BUILD_MICROBENCH)
  include(FetchContent)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
  )
  FetchContent_MakeAvailable(googlebenchmark)

  add_executable(arb_microbench microbench_main.cpp)
  target_link_libraries(arb_microbench PRIVATE arb_core benchmark::benchmark)
endif()
