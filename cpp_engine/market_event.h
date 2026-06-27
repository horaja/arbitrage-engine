#pragma once

#include <cstdint>
#include <optional>
#include <type_traits>

enum class MarketEventType {
  TopOfBookQuote
};

struct TopOfBookQuote {
  double bid_price = 0.0;
  double bid_size = 0.0;
  double ask_price = 0.0;
  double ask_size = 0.0;
};

// MarketEvent is the payload that crosses the SPSC queue between the producer
// (parse + interning) and the consumer (book + graph + detection). It is kept
// allocation-free and trivially copyable: the symbol is interned to an integer
// id at the adapter (see SymbolRegistry), and timestamps are parsed to integer
// nanoseconds. Strings never flow through the hot path.
struct MarketEvent {
  std::int64_t exchange_time_ns = 0;
  std::int64_t receive_time_ns = 0;
  MarketEventType event_type = MarketEventType::TopOfBookQuote;
  std::uint32_t symbol_id = 0;
  std::optional<TopOfBookQuote> top_of_book;
};

static_assert(std::is_trivially_copyable<MarketEvent>::value,
              "MarketEvent must stay trivially copyable so the SPSC queue moves "
              "it without per-element allocation.");
