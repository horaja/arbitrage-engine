#include "replay_adapter.h"

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string_view>

namespace {

std::string_view trim(std::string_view value) {
  const char* whitespace = " \t\r\n";
  const std::size_t start = value.find_first_not_of(whitespace);
  if (start == std::string_view::npos) {
    return std::string_view{};
  }
  const std::size_t end = value.find_last_not_of(whitespace);
  return value.substr(start, end - start + 1);
}

// Splits `line` into exactly `expected` trimmed comma-separated columns.
// Returns false if the column count differs (too few or extra columns).
bool split_columns(const std::string& line, std::string_view* fields, std::size_t expected) {
  std::string_view view(line);
  std::size_t count = 0;
  std::size_t pos = 0;
  while (true) {
    const std::size_t comma = view.find(',', pos);
    const std::string_view column =
        view.substr(pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);
    if (count >= expected) {
      return false;  // extra columns
    }
    fields[count++] = trim(column);
    if (comma == std::string_view::npos) {
      break;
    }
    pos = comma + 1;
  }
  return count == expected;
}

bool parse_double(std::string_view value, double& out) {
  char buffer[64];
  if (value.empty() || value.size() >= sizeof(buffer)) {
    return false;
  }
  std::memcpy(buffer, value.data(), value.size());
  buffer[value.size()] = '\0';

  errno = 0;
  char* end = nullptr;
  const double parsed = std::strtod(buffer, &end);
  if (end != buffer + value.size() || errno != 0) {
    return false;
  }
  out = parsed;
  return true;
}

// Days since the Unix epoch (1970-01-01) for a proleptic-Gregorian y/m/d.
// Howard Hinnant's days_from_civil.
std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d) {
  y -= m <= 2;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

bool all_digits(std::string_view value) {
  for (const char c : value) {
    if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
      return false;
    }
  }
  return !value.empty();
}

bool parse_uint(std::string_view value, unsigned& out) {
  unsigned result = 0;
  for (const char c : value) {
    if (std::isdigit(static_cast<unsigned char>(c)) == 0) {
      return false;
    }
    result = result * 10 + static_cast<unsigned>(c - '0');
  }
  out = result;
  return true;
}

// Parses a timestamp to integer nanoseconds since the Unix epoch. Accepts either
// a bare numeric nanosecond value or the fixed ISO 8601 shape
// "YYYY-MM-DDTHH:MM:SS" with an optional trailing 'Z'. Anything else fails,
// preserving the adapter's malformed-row contract.
bool parse_timestamp_ns(std::string_view value, std::int64_t& out) {
  if (value.empty()) {
    return false;
  }

  if (all_digits(value)) {
    char buffer[32];
    if (value.size() >= sizeof(buffer)) {
      return false;
    }
    std::memcpy(buffer, value.data(), value.size());
    buffer[value.size()] = '\0';
    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(buffer, &end, 10);
    if (end != buffer + value.size() || errno != 0) {
      return false;
    }
    out = static_cast<std::int64_t>(parsed);
    return true;
  }

  if (!value.empty() && value.back() == 'Z') {
    value.remove_suffix(1);
  }

  std::int64_t fraction_ns = 0;
  const std::size_t dot_pos = value.find('.');
  if (dot_pos != std::string_view::npos) {
    const std::string_view fraction = value.substr(dot_pos + 1);
    value = value.substr(0, dot_pos);
    if (fraction.empty() || fraction.size() > 9 || !all_digits(fraction)) {
      return false;
    }
    unsigned fraction_value = 0;
    if (!parse_uint(fraction, fraction_value)) {
      return false;
    }
    std::int64_t scaled = fraction_value;
    for (std::size_t digit = fraction.size(); digit < 9; ++digit) {
      scaled *= 10;
    }
    fraction_ns = scaled;
  }

  if (value.size() != 19 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
      value[13] != ':' || value[16] != ':') {
    return false;
  }

  unsigned year = 0;
  unsigned month = 0;
  unsigned day = 0;
  unsigned hour = 0;
  unsigned minute = 0;
  unsigned second = 0;
  if (!parse_uint(value.substr(0, 4), year) || !parse_uint(value.substr(5, 2), month) ||
      !parse_uint(value.substr(8, 2), day) || !parse_uint(value.substr(11, 2), hour) ||
      !parse_uint(value.substr(14, 2), minute) || !parse_uint(value.substr(17, 2), second)) {
    return false;
  }
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60) {
    return false;
  }

  const std::int64_t days = days_from_civil(static_cast<std::int64_t>(year), month, day);
  const std::int64_t epoch_seconds = days * 86400 + hour * 3600 + minute * 60 + second;
  out = epoch_seconds * 1000000000LL + fraction_ns;
  return true;
}

}  // namespace

CsvReplayAdapter::CsvReplayAdapter(std::string input_path) : input_path_(std::move(input_path)) {}

bool CsvReplayAdapter::open(std::string& error_message) {
  metadata_ = ReplayMetadata{};
  registry_ = SymbolRegistry{};
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

  std::string line;
  while (std::getline(scan_stream, line)) {
    MarketEvent event;
    if (!parse_row(line, /*register_symbols=*/true, event, error_message)) {
      return false;
    }
    ++metadata_.event_count;
  }

  if (metadata_.event_count == 0) {
    error_message = "Input file does not contain any replay rows: " + input_path_;
    return false;
  }

  metadata_.symbols = registry_.symbol_names();

  if (!reset_stream(error_message)) {
    return false;
  }

  is_open_ = true;
  return true;
}

const ReplayMetadata& CsvReplayAdapter::metadata() const {
  return metadata_;
}

const SymbolRegistry& CsvReplayAdapter::registry() const {
  return registry_;
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

  return parse_row(line, /*register_symbols=*/false, event, error_message);
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

bool CsvReplayAdapter::parse_row(const std::string& line, bool register_symbols, MarketEvent& event,
                                 std::string& error_message) {
  std::string_view fields[6];
  if (!split_columns(line, fields, 6)) {
    error_message = "Malformed CSV row: " + line;
    return false;
  }

  const std::string_view symbol_field = fields[1];
  if (symbol_field.empty()) {
    error_message = "Encountered an empty symbol in CSV row: " + line;
    return false;
  }

  std::int64_t exchange_time_ns = 0;
  double bid_price = 0.0;
  double bid_size = 0.0;
  double ask_price = 0.0;
  double ask_size = 0.0;
  if (!parse_timestamp_ns(fields[0], exchange_time_ns) || !parse_double(fields[2], bid_price) ||
      !parse_double(fields[3], bid_size) || !parse_double(fields[4], ask_price) ||
      !parse_double(fields[5], ask_size)) {
    error_message = "Failed to parse CSV row: " + line;
    return false;
  }

  const std::string symbol(symbol_field);
  std::uint32_t symbol_id = 0;
  if (register_symbols) {
    if (!registry_.intern_symbol(symbol, symbol_id, error_message)) {
      return false;
    }
  } else if (!registry_.find_symbol(symbol, symbol_id)) {
    error_message = "Encountered an unregistered symbol in CSV row: " + line;
    return false;
  }

  event.exchange_time_ns = exchange_time_ns;
  event.receive_time_ns = exchange_time_ns;
  event.event_type = MarketEventType::TopOfBookQuote;
  event.symbol_id = symbol_id;
  event.top_of_book = TopOfBookQuote{bid_price, bid_size, ask_price, ask_size};
  return true;
}
