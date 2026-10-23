/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <sstream>
#include <thread>

#include "ring_buffer.hpp"

/*
 * api/matching_engine_api.h
 */

typedef enum : uint8_t {
  /* deprecated */ ORDER_ACK = 0,
  ORDER_ACCEPTED = 0,

  /* deprecated */ TRADE = 1,
  ORDER_TRADED = 1,

  ORDER_CANCELED = 2,
  CANCEL_REJECTED = 4,

  /* EXTENSION */

  ORDER_REJECT = 6,
  LEVEL_CREATE = 7,
  LEVEL_REMOVE = 8,
} Type;

/*
 * Event
 */

struct Event {
  /* int8_t  */ Type type{0};
  /* int8_t  */ Side takerSide{0};
  /* int8_t  */ Side makerSide{0};

  /* int64_t */ Id takerId{0};
  /* int64_t */ Id makerId{0};

  /* int32_t */ Index takerSlot{0};
  /* int32_t */ Index makerSlot{0};

  /* int32_t */ Price takerPrice{0};
  /* int32_t */ Price makerPrice{0};

  /* int32_t */ Qty takerCumQty{0};
  /* int32_t */ Qty makerCumQty{0};

  /* int32_t */ Qty takerLeavesQty{0};
  /* int32_t */ Qty makerLeavesQty{0};

  /* int32_t */ Price lastPrice{0};
  /* int32_t */ Qty lastQty{0};
};

static_assert(sizeof(Event) == 64);

inline bool operator==(const Event& lhs, const Event& rhs) noexcept {
  return true;
}

inline Event Trade(Id maker, Id taker, Price lastPrice, Qty lastQty) noexcept {
  return {.type = ORDER_TRADED, //
          .takerId = taker, 
          .makerId = maker, 
          .lastPrice = lastPrice, 
          .lastQty = lastQty};
}

inline Event OrderAdded(Id id, Index slot, Side side, Price orderPrice, Qty leavesQty) noexcept {
  return {.type = ORDER_ACCEPTED, //
          .makerSide = side, 
          .makerId = id, 
          .makerSlot = slot, 
          .makerPrice = orderPrice, 
          .makerLeavesQty = leavesQty};
}

inline Event OrderReject(Id id) noexcept {
  return {.type = ORDER_REJECT, .makerId = id};
}

inline Event OrderCanceled(Id id, Index slot, Side side, Price orderPrice, Qty cumQty) noexcept {
  return {.type = ORDER_CANCELED, //
          .makerSide = side, 
          .makerId = id, 
          .makerSlot = slot, 
          .makerPrice = orderPrice, 
          .makerCumQty = cumQty};
}

inline Event CancelReject(Id id, Index slot, Side side, Price orderPrice, Id req = -1) noexcept {
  return {.type = CANCEL_REJECTED, //
          .makerSide = side, 
          .takerId = req, 
          .makerId = id, 
          .makerSlot = slot, 
          .makerPrice = orderPrice};
}

inline Event LevelCreate(Price lastPrice) noexcept {
  return {.type = LEVEL_CREATE, .lastPrice = lastPrice};
}

inline Event LevelRemove(Price lastPrice) noexcept {
  return {.type = LEVEL_REMOVE, .lastPrice = lastPrice};
}

std::string to_string(const Event& e) noexcept {
  std::ostringstream oss;

  oss << "Event{"
      << "type=" << static_cast<int>(e.type) << ", "

      << "taker{id=" << e.takerId << ", slot=" << e.takerSlot << ", side=" << (int)e.takerSide << ", price=" << e.takerPrice
                      << ", cumQty=" << e.takerCumQty << ", leavesQty=" << e.takerLeavesQty << "}, "

      << "maker{id=" << e.makerId << ", slot=" << e.makerSlot << ", side=" << (int)e.makerSide << ", price=" << e.makerPrice
                      << ", cumQty=" << e.makerCumQty << ", leavesQty=" << e.makerLeavesQty << "}, "
                
      << "lastPrice=" << e.lastPrice << ", "
      << "lastQty=" << e.lastQty << "}";

  return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Event& e) noexcept {
  return os << to_string(e);
}

/*
 * Default EventHandler
 */

struct EventHandler {
  void onOrderAccepted(Id id, Index slot, Side side, Price price, Qty leavesQty) {
    buffer.push(OrderAdded(id, slot, side, price, leavesQty));
  }

  void onOrderCanceled(Id id, Index slot, Side side, Price price, Qty cumQty, Id req = -1) {
    buffer.push(OrderCanceled(id, slot, side, price, cumQty));
  }

  void onCancelRejected(Id id, Index slot, Side side, Price price, Id req = -1) {
    buffer.push(CancelReject(id, slot, side, price, req));
  }

  void onTrade(
    Id takerId, Index takerSlot, Side takerSide, Price takerPrice, Qty takerCumQty, Qty takerLeavesQty,
    Id makerId, Index makerSlot, Side makerSide, Price makerPrice, Qty makerCumQty, Qty makerLeavesQty,
    Price lastPrice, Qty lastQty
  ) {
    buffer.push(
        Event{
            .type = ORDER_TRADED, //
            .takerSide = takerSide,
            .makerSide = makerSide,
            .takerId = takerId,
            .makerId = makerId,
            .takerSlot = takerSlot,
            .makerSlot = makerSlot,
            .takerPrice = takerPrice,
            .makerPrice = makerPrice,
            .takerCumQty = takerCumQty,
            .makerCumQty = makerCumQty,
            .takerLeavesQty = takerLeavesQty,
            .makerLeavesQty = makerLeavesQty,
            .lastPrice = lastPrice,
            .lastQty = lastQty});
  }

  /* V2 extension */
  
  void orderReject(Id id) {
    buffer.push(OrderReject(id));
  }

  void levelCreate(Price price) {
    buffer.push(LevelCreate(price));
  }

  void levelRemove(Price price) {
    buffer.push(LevelRemove(price));
  }

  /*
   * Tests extension
   */

  Event pop() {
    Event event;

    while (buffer.pop(event) == false) {
      _mm_pause();
    }

    return event;
  }

  std::size_t clear() noexcept {
    Event event;
    std::size_t count = 0;

    while (buffer.empty() == false) {
      count += 1;
      buffer.pop(event);
    }

    return count;
  }

  std::size_t log(const std::string& prefix = "") {
    std::size_t count = 0;

    while (buffer.empty() == false) {
      count += 1;

      Event event;
      buffer.pop(event);

      std::cout << prefix << event << std::endl;
    }

    return count;
  }

  RingBuffer<Event, 128> buffer;
};
