#pragma once

#include <chrono>
#include <optional>

#include "market_event.h"

enum class ClockMode {
  ReplayTime,
  WallTime
};

class Clock {
public:
  virtual ~Clock() = default;
  virtual void before_event(const std::optional<MarketEvent>& previous_event, const MarketEvent& next_event) = 0;
};

class LogicalReplayClock : public Clock {
public:
  void before_event(const std::optional<MarketEvent>& previous_event, const MarketEvent& next_event) override;
};

class FixedDelayClock : public Clock {
public:
  explicit FixedDelayClock(std::chrono::milliseconds event_delay);
  void before_event(const std::optional<MarketEvent>& previous_event, const MarketEvent& next_event) override;

private:
  std::chrono::milliseconds event_delay_;
};
