#pragma once

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include "market_event.h"
#include "symbol_registry.h"

struct ReplayMetadata {
  std::vector<std::string> symbols;
  std::size_t event_count = 0;
};

class ReplayAdapter {
public:
  virtual ~ReplayAdapter() = default;

  virtual bool open(std::string& error_message) = 0;
  virtual const ReplayMetadata& metadata() const = 0;
  virtual const SymbolRegistry& registry() const = 0;
  virtual bool next_event(MarketEvent& event, std::string& error_message) = 0;
};

class CsvReplayAdapter : public ReplayAdapter {
public:
  explicit CsvReplayAdapter(std::string input_path);

  bool open(std::string& error_message) override;
  const ReplayMetadata& metadata() const override;
  const SymbolRegistry& registry() const override;
  bool next_event(MarketEvent& event, std::string& error_message) override;

private:
  bool reset_stream(std::string& error_message);
  // Parses one CSV row into `event`. When `register_symbols` is true (the
  // open() scan) new symbols are interned; otherwise the symbol must already be
  // known. Allocation-free on the steady-state path apart from the symbol
  // lookup key.
  bool parse_row(const std::string& line, bool register_symbols, MarketEvent& event, std::string& error_message);

  std::string input_path_;
  ReplayMetadata metadata_;
  SymbolRegistry registry_;
  std::ifstream input_stream_;
  bool is_open_ = false;
};
