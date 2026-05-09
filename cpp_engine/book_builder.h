#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "market_event.h"

class BookBuilder {
public:
  bool apply_quote(const std::string& symbol, const TopOfBookQuote& quote);

  const TopOfBookQuote* latest(const std::string& symbol) const;

  bool all_available(const std::vector<std::string>& symbols) const;

  static bool is_valid(const TopOfBookQuote& quote);

private:
  std::unordered_map<std::string, TopOfBookQuote> latest_by_symbol_;
};
