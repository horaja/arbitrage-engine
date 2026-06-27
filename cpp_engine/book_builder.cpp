#include "book_builder.h"

BookBuilder::BookBuilder(std::size_t symbol_count)
    : latest_by_symbol_(symbol_count), has_quote_(symbol_count, false) {}

bool BookBuilder::is_valid(const TopOfBookQuote& quote) {
  if (quote.bid_price <= 0.0 || quote.bid_size <= 0.0 ||
      quote.ask_price <= 0.0 || quote.ask_size <= 0.0) {
    return false;
  }
  return quote.bid_price < quote.ask_price;
}

bool BookBuilder::apply_quote(std::uint32_t symbol_id, const TopOfBookQuote& quote) {
  if (!is_valid(quote)) {
    return false;
  }
  latest_by_symbol_[symbol_id] = quote;
  has_quote_[symbol_id] = true;
  return true;
}

const TopOfBookQuote* BookBuilder::latest(std::uint32_t symbol_id) const {
  if (symbol_id >= has_quote_.size() || !has_quote_[symbol_id]) {
    return nullptr;
  }
  return &latest_by_symbol_[symbol_id];
}

bool BookBuilder::all_available(const std::vector<std::uint32_t>& symbol_ids) const {
  for (const std::uint32_t symbol_id : symbol_ids) {
    if (latest(symbol_id) == nullptr) {
      return false;
    }
  }
  return true;
}
