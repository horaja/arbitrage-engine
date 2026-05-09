#include "clock.h"

#include <thread>

void LogicalReplayClock::before_event(
    const std::optional<MarketEvent>& previous_event,
    const MarketEvent& next_event) {
  (void) previous_event;
  (void) next_event;
}

FixedDelayClock::FixedDelayClock(std::chrono::milliseconds event_delay) : event_delay_(event_delay) {}

void FixedDelayClock::before_event(
    const std::optional<MarketEvent>& previous_event,
    const MarketEvent& next_event) {
  (void) next_event;
  if (!previous_event || event_delay_.count() <= 0) {
    return;
  }

  std::this_thread::sleep_for(event_delay_);
}
