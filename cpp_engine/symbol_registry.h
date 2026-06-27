#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Pre-resolved identity for one trading pair "BASE-QUOTE". Built once at adapter
// open() so the hot path never parses or hashes symbol strings again.
struct SymbolDescriptor {
  std::uint32_t symbol_id = 0;
  std::uint32_t base_currency_id = 0;
  std::uint32_t quote_currency_id = 0;
};

// Interns currency names and trading-pair symbols to dense integer ids. All
// lookups used on the hot path are O(1) array indexing; string hashing only
// happens during interning (adapter open) and rare name resolution (reporting).
class SymbolRegistry {
public:
  // Registers a "BASE-QUOTE" symbol (and its two currencies) if new, returning
  // its id. Idempotent for repeated symbols. Returns false on a malformed
  // symbol (missing '-' or an empty side).
  bool intern_symbol(const std::string& symbol, std::uint32_t& symbol_id, std::string& error_message);

  // Resolves an already-registered symbol; false if unknown.
  bool find_symbol(const std::string& symbol, std::uint32_t& symbol_id) const;

  // Resolves an already-registered currency; false if unknown.
  bool find_currency(const std::string& name, std::uint32_t& currency_id) const;

  std::size_t symbol_count() const { return symbol_descriptors_.size(); }
  std::size_t currency_count() const { return currency_names_.size(); }

  const SymbolDescriptor& descriptor(std::uint32_t symbol_id) const { return symbol_descriptors_[symbol_id]; }
  const std::string& symbol_name(std::uint32_t symbol_id) const { return symbol_names_[symbol_id]; }
  const std::string& currency_name(std::uint32_t currency_id) const { return currency_names_[currency_id]; }

  // First-seen ordered list of symbol names (mirrors prior ReplayMetadata order).
  const std::vector<std::string>& symbol_names() const { return symbol_names_; }

private:
  std::uint32_t intern_currency(const std::string& name);

  std::unordered_map<std::string, std::uint32_t> currency_to_id_;
  std::vector<std::string> currency_names_;
  std::unordered_map<std::string, std::uint32_t> symbol_to_id_;
  std::vector<std::string> symbol_names_;
  std::vector<SymbolDescriptor> symbol_descriptors_;
};
