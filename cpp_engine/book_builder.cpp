#include "book_builder.h"

bool BookBuilder::is_valid(const TopOfBookQuote& quote) {
  if (quote.bid_price <= 0.0 || quote.bid_size <= 0.0 ||
      quote.ask_price <= 0.0 || quote.ask_size <= 0.0) {
    return false;
  }
  return quote.bid_price < quote.ask_price;
}

bool BookBuilder::apply_quote(const std::string& symbol, const TopOfBookQuote& quote) {
  if (!is_valid(quote)) {
    return false;
  }
  latest_by_symbol_[symbol] = quote;
  return true;
}

const TopOfBookQuote* BookBuilder::latest(const std::string& symbol) const {
  const auto iter = latest_by_symbol_.find(symbol);
  if (iter == latest_by_symbol_.end()) {
    return nullptr;
  }
  return &iter->second;
}

bool BookBuilder::all_available(const std::vector<std::string>& symbols) const {
  for (const auto& symbol : symbols) {
    if (latest_by_symbol_.find(symbol) == latest_by_symbol_.end()) {
      return false;
    }
  }
  return true;
}
