/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cstdint>
#include <string>

#include "common.hpp"

using WireMessage = std::string;

enum class OrderCommandType {
  LIMIT,
  IOC,
  MARKET,
  CANCEL,
};

struct OrderCommand {
  OrderCommandType type{};
  Side side{};
  Id clOrdId{};
  Id orderId{};
  Id origClOrdId{};
  Qty qty{};
  Price price{};
  uint64_t sequenceNumber{};
};

enum class OrderEventType {
  ACCEPTED,
  TRADED,
  CANCELED,
  CANCEL_REJECTED,
};

struct OrderEvent {
  OrderEventType type{};
  Side side{};
  Id clOrdId{};
  Id orderId{};
  Id origClOrdId{};
  Price orderPrice{};
  Qty cumQty{};
  Qty leavesQty{};
  Price lastPrice{};
  Qty lastQty{};
  uint64_t sequenceNumber{};
  char status{};
};
