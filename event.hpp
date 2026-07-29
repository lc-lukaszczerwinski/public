/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <sstream>
#include <thread>

/*
 * api/matching_engine_api.h
 */

typedef enum : uint8_t {
    ORDER_ACK     = 0,
    TRADE         = 1,
    CANCEL_ACK    = 2,
    MODIFY_ACK    = 3,
    CANCEL_REJECT = 4,   /* cancel of an order that is not resting */
    MODIFY_REJECT = 5    /* modify of an order that is not resting */
} Type;

/*
 * Event
 */

struct Event {
  /* int8_t  */ Type type{0};
  /* int8_t  */ Side side{0};
  /* int48_t */ char padding1[6]{0};
  
  /* int64_t */ Id seq{0};
  /* int64_t */ Id maker{0};
  /* int64_t */ Id taker{0};
  
  /* int32_t */ Price price{0};
  /* int32_t */ Qty qty{0};

  /* int192_t */ char padding2[24]{0};
};

static_assert(sizeof(Event) == 64);

inline bool operator==(const Event& lhs, const Event& rhs)  noexcept {
  return true;
}

inline Event Trade(Id seq, Id maker, Id taker, Price price, Qty qty)  noexcept {
  return {.type = TRADE, .maker = maker, .taker = taker, .price = price, .qty = qty};
}

inline Event OrderACK(Id seq, Id id, Side side, Price price, Qty qty)  noexcept {
  return {.type = ORDER_ACK, .side = side, .seq = seq, .maker = id, .price = price, .qty = qty};
}

inline Event ModifyACK(Id seq, Id id, Side side, Price newPrice, Qty newQty)  noexcept {
  return {.type = MODIFY_ACK, .side = side, .seq = seq, .maker = id, .price = newPrice, .qty = newQty};
}

inline Event ModifyReject(Id seq, Id id)  noexcept {
  return {.type = MODIFY_REJECT, .seq = seq, .maker = id};
}

inline Event CancelACK(Id seq, Id id, Side side, Price price, Qty qty)  noexcept {
  return {.type = CANCEL_ACK, .side = side, .seq = seq, .maker = id, .price = price, .qty = qty};
}

inline Event CancelReject(Id seq, Id id)  noexcept {
  return {.type = CANCEL_REJECT, .seq = seq, .maker = id};
}

std::string to_string(const Event& e)  noexcept {
  std::ostringstream oss;

  oss << "Event{"
      << "type=" << static_cast<int>(e.type) << ", "
      << "side=" << static_cast<int>(e.side) << ", "
      << "seq=" << e.seq << ", "
      << "maker=" << e.maker << ", "
      << "taker=" << e.taker << ", "
      << "price=" << e.price << ", "
      << "qty=" << e.qty
      << "}";
      
  return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Event& e)  noexcept {
  return os << to_string(e);
}
