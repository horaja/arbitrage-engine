#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "market_event.h"

// Maintains the latest valid top-of-book per symbol. Symbols are addressed by
// the dense integer ids assigned by SymbolRegistry, so lookups and updates are
// O(1) array access with no hashing or steady-state reallocation.
class BookBuilder {
public:
  explicit BookBuilder(std::size_t symbol_count);

  bool apply_quote(std::uint32_t symbol_id, const TopOfBookQuote& quote);

  const TopOfBookQuote* latest(std::uint32_t symbol_id) const;

  bool all_available(const std::vector<std::uint32_t>& symbol_ids) const;

  static bool is_valid(const TopOfBookQuote& quote);

private:
  std::vector<TopOfBookQuote> latest_by_symbol_;
  std::vector<bool> has_quote_;
};
