#pragma once

#include <optional>
#include <string>

enum class MarketEventType {
  TradeTick
};

struct TradeTick {
  double price = 0.0;
  double quantity = 0.0;
};

struct MarketEvent {
  std::string exchange_timestamp;
  std::string receive_timestamp;
  MarketEventType event_type = MarketEventType::TradeTick;
  std::string symbol;
  std::optional<TradeTick> trade_tick;
};
