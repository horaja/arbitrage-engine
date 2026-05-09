#pragma once

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>

#include "market_event.h"

struct ReplayMetadata {
  std::vector<std::string> symbols;
  std::size_t event_count = 0;
};

class ReplayAdapter {
public:
  virtual ~ReplayAdapter() = default;

  virtual bool open(std::string& error_message) = 0;
  virtual const ReplayMetadata& metadata() const = 0;
  virtual bool next_event(MarketEvent& event, std::string& error_message) = 0;
};

class CsvReplayAdapter : public ReplayAdapter {
public:
  explicit CsvReplayAdapter(std::string input_path);

  bool open(std::string& error_message) override;
  const ReplayMetadata& metadata() const override;
  bool next_event(MarketEvent& event, std::string& error_message) override;

private:
  bool reset_stream(std::string& error_message);
  static bool parse_row(const std::string& line, MarketEvent& event, std::string& error_message);

  std::string input_path_;
  ReplayMetadata metadata_;
  std::ifstream input_stream_;
  bool is_open_ = false;
};
