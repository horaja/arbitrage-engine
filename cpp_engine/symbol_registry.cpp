#include "symbol_registry.h"

std::uint32_t SymbolRegistry::intern_currency(const std::string& name) {
  const auto iter = currency_to_id_.find(name);
  if (iter != currency_to_id_.end()) {
    return iter->second;
  }
  const auto id = static_cast<std::uint32_t>(currency_names_.size());
  currency_to_id_.emplace(name, id);
  currency_names_.push_back(name);
  return id;
}

bool SymbolRegistry::intern_symbol(const std::string& symbol, std::uint32_t& symbol_id, std::string& error_message) {
  const auto existing = symbol_to_id_.find(symbol);
  if (existing != symbol_to_id_.end()) {
    symbol_id = existing->second;
    return true;
  }

  const std::size_t delimiter_pos = symbol.find('-');
  if (delimiter_pos == std::string::npos || delimiter_pos == 0 || delimiter_pos + 1 >= symbol.size()) {
    error_message = "Invalid symbol format. Expected 'BASE-QUOTE', but received: '" + symbol + "'";
    return false;
  }

  const std::string base = symbol.substr(0, delimiter_pos);
  const std::string quote = symbol.substr(delimiter_pos + 1);

  SymbolDescriptor descriptor;
  descriptor.symbol_id = static_cast<std::uint32_t>(symbol_descriptors_.size());
  descriptor.base_currency_id = intern_currency(base);
  descriptor.quote_currency_id = intern_currency(quote);

  symbol_id = descriptor.symbol_id;
  symbol_to_id_.emplace(symbol, symbol_id);
  symbol_names_.push_back(symbol);
  symbol_descriptors_.push_back(descriptor);
  return true;
}

bool SymbolRegistry::find_symbol(const std::string& symbol, std::uint32_t& symbol_id) const {
  const auto iter = symbol_to_id_.find(symbol);
  if (iter == symbol_to_id_.end()) {
    return false;
  }
  symbol_id = iter->second;
  return true;
}

bool SymbolRegistry::find_currency(const std::string& name, std::uint32_t& currency_id) const {
  const auto iter = currency_to_id_.find(name);
  if (iter == currency_to_id_.end()) {
    return false;
  }
  currency_id = iter->second;
  return true;
}
