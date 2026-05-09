#pragma once

#include <optional>
#include <string>

enum class MarketEventType {
  TopOfBookQuote
};

struct TopOfBookQuote {
  double bid_price = 0.0;
  double bid_size = 0.0;
  double ask_price = 0.0;
  double ask_size = 0.0;
};

struct MarketEvent {
  std::string exchange_timestamp;
  std::string receive_timestamp;
  MarketEventType event_type = MarketEventType::TopOfBookQuote;
  std::string symbol;
  std::optional<TopOfBookQuote> top_of_book;
};
