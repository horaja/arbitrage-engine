#include "replay_adapter.h"

#include <fstream>
#include <sstream>
#include <unordered_set>

namespace {

std::string trim(const std::string& value) {
  const std::size_t start = value.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }

  const std::size_t end = value.find_last_not_of(" \t\r\n");
  return value.substr(start, end - start + 1);
}

}  // namespace

CsvReplayAdapter::CsvReplayAdapter(std::string input_path) : input_path_(std::move(input_path)) {}

bool CsvReplayAdapter::open(std::string& error_message) {
  metadata_ = ReplayMetadata{};
  std::ifstream scan_stream(input_path_);
  if (!scan_stream.is_open()) {
    error_message = "Could not open input file: " + input_path_;
    return false;
  }

  std::string header_line;
  if (!std::getline(scan_stream, header_line)) {
    error_message = "Input file is empty: " + input_path_;
    return false;
  }

  std::unordered_set<std::string> seen_symbols;
  std::string line;
  while (std::getline(scan_stream, line)) {
    MarketEvent event;
    if (!parse_row(line, event, error_message)) {
      return false;
    }

    ++metadata_.event_count;
    if (seen_symbols.insert(event.symbol).second) {
      metadata_.symbols.push_back(event.symbol);
    }
  }

  if (metadata_.event_count == 0) {
    error_message = "Input file does not contain any replay rows: " + input_path_;
    return false;
  }

  if (!reset_stream(error_message)) {
    return false;
  }

  is_open_ = true;
  return true;
}

const ReplayMetadata& CsvReplayAdapter::metadata() const {
  return metadata_;
}

bool CsvReplayAdapter::next_event(MarketEvent& event, std::string& error_message) {
  if (!is_open_) {
    error_message = "Replay adapter must be opened before reading events.";
    return false;
  }

  std::string line;
  if (!std::getline(input_stream_, line)) {
    error_message.clear();
    return false;
  }

  return parse_row(line, event, error_message);
}

bool CsvReplayAdapter::reset_stream(std::string& error_message) {
  input_stream_.close();
  input_stream_.clear();
  input_stream_.open(input_path_);
  if (!input_stream_.is_open()) {
    error_message = "Could not reopen input file: " + input_path_;
    return false;
  }

  std::string header_line;
  if (!std::getline(input_stream_, header_line)) {
    error_message = "Input file is empty: " + input_path_;
    return false;
  }

  return true;
}

bool CsvReplayAdapter::parse_row(const std::string& line, MarketEvent& event, std::string& error_message) {
  std::stringstream row_stream(line);
  std::string timestamp_field;
  std::string symbol_field;
  std::string bid_price_field;
  std::string bid_size_field;
  std::string ask_price_field;
  std::string ask_size_field;
  std::string trailing_field;

  if (!std::getline(row_stream, timestamp_field, ',') ||
      !std::getline(row_stream, symbol_field, ',') ||
      !std::getline(row_stream, bid_price_field, ',') ||
      !std::getline(row_stream, bid_size_field, ',') ||
      !std::getline(row_stream, ask_price_field, ',') ||
      !std::getline(row_stream, ask_size_field, ',')) {
    error_message = "Malformed CSV row: " + line;
    return false;
  }

  if (std::getline(row_stream, trailing_field, ',')) {
    error_message = "Malformed CSV row (extra columns): " + line;
    return false;
  }

  try {
    event.exchange_timestamp = trim(timestamp_field);
    event.receive_timestamp = event.exchange_timestamp;
    event.event_type = MarketEventType::TopOfBookQuote;
    event.symbol = trim(symbol_field);
    event.top_of_book = TopOfBookQuote{
        std::stod(trim(bid_price_field)),
        std::stod(trim(bid_size_field)),
        std::stod(trim(ask_price_field)),
        std::stod(trim(ask_size_field)),
    };
  } catch (const std::exception& exception) {
    error_message = "Failed to parse CSV row: " + line + " (" + exception.what() + ")";
    return false;
  }

  if (event.symbol.empty()) {
    error_message = "Encountered an empty symbol in CSV row: " + line;
    return false;
  }

  return true;
}
