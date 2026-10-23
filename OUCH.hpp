/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cassert>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "common.hpp"

namespace OUCH {

constexpr char MsgType_EnterOrder = 'O';
constexpr char MsgType_CancelOrder = 'X';
constexpr char MsgType_Accepted = 'A';
constexpr char MsgType_Executed = 'E';
constexpr char MsgType_Canceled = 'C';
constexpr char MsgType_Rejected = 'J';

constexpr char Buy = 'B';
constexpr char Sell = 'S';

constexpr uint32_t TimeInForce_GTC = 1;
constexpr uint32_t TimeInForce_IOC = 3;

constexpr std::size_t TokenLength = 14;
constexpr std::size_t StockLength = 8;
constexpr std::string_view Stock = "HFTDEMO ";

constexpr char RejectionReason_Cancel = 'C';

class Writer {
public:

  void u8(char value) {
    _message.push_back(value);
  }

  void u32(uint32_t value) {
    _message.push_back(static_cast<char>(value >> 24));
    _message.push_back(static_cast<char>(value >> 16));
    _message.push_back(static_cast<char>(value >> 8));
    _message.push_back(static_cast<char>(value));
  }

  void u64(uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
      _message.push_back(static_cast<char>(value >> shift));
    }
  }

  void i64(int64_t id) {
    assert(id >= 0);

    char digits[TokenLength]{};
    const auto [end, error] = std::to_chars(std::begin(digits), std::end(digits), id);
    assert(error == std::errc{});

    for (char* current = digits; current != end; ++current) {
      _message.push_back(*current);
    }

    for (std::size_t padding = end - std::begin(digits); padding < TokenLength; ++padding) {
      _message.push_back(' ');
    }
  }

  void string(std::string_view value, std::size_t length) {
    assert(value.size() <= length);
    _message.append(value);
    _message.append(length - value.size(), ' ');
  }

  std::string finish() {
    return std::move(_message);
  }

private:

  std::string _message;
};

class Reader {
public:

  explicit Reader(std::string_view message)
    : _message(message) //
  {
  }

  std::optional<char> u8() {
    if (_offset == _message.size()) {
      return std::nullopt;
    }

    return _message[_offset++];
  }

  std::optional<uint32_t> u32() {
    if (_message.size() - _offset < sizeof(uint32_t)) {
      return std::nullopt;
    }

    uint32_t value = 0;

    for (int byte = 0; byte < 4; ++byte) {
      value = (uint32_t)(value << 8) | (unsigned char)(_message[_offset++]);
    }

    return value;
  }

  std::optional<uint64_t> u64() {
    if (_message.size() - _offset < sizeof(uint64_t)) {
      return std::nullopt;
    }

    uint64_t value = 0;

    for (int byte = 0; byte < 8; ++byte) {
      value = (value << 8) | (unsigned char)(_message[_offset++]);
    }

    return value;
  }

  std::optional<int64_t> i64() {
    if (_message.size() - _offset < TokenLength) {
      return std::nullopt;
    }

    std::string_view token = _message.substr(_offset, TokenLength);
    _offset += TokenLength;

    while (! token.empty() && token.back() == ' ') {
      token.remove_suffix(1);
    }

    if (token.empty()) {
      return std::nullopt;
    }

    int64_t value{};
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), ::ref(value));

    if (error != std::errc{} || end != token.data() + token.size() || value < 0) {
      return std::nullopt;
    }

    return value;
  }

  std::optional<std::string_view> string(std::size_t length) {
    if (_message.size() - _offset < length) {
      return std::nullopt;
    }

    const auto value = _message.substr(_offset, length);
    _offset += length;
    return value;
  }

  bool empty() const {
    return _offset == _message.size();
  }

private:

  std::string_view _message;
  std::size_t _offset{};
};

} // namespace OUCH
