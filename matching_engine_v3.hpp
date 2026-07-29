/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <immintrin.h>
#include <iostream>

#include "common.hpp"
#include "intrusive_list.hpp"
#include "intrusive_treap.hpp"
#include "object_pool.hpp"
#include "random.hpp"
#include "ring_buffer_spsc.hpp"

namespace v3 {

/*
 * Request represents an order request in the matching engine.
 */

struct Request {
  uint32_t id;
  int32_t price; // {price!=0 : limit}, {price=0 : market}
  int32_t qty;   // {qty>0 : buy}, {qty<0 : sell}, {qty=0, price>0 : cancel buy}, {qty=0, price<0 : cancel sell}
};

std::string to_string(const Request& request) {
  std::ostringstream os;

  std::string type;

  if(request.price == 0 && request.qty > 0) {
    type = "Market side:Buy";
  } else if(request.price == 0 && request.qty < 0) {
    type = "Market side:Sell";
  } else if(request.price > 0 && request.qty > 0) {
    type = "Limit side:Buy";
  } else if(request.price > 0 && request.qty < 0) {
    type = "Limit side:Sell";
  } else if(request.price < 0 && request.qty == 0) {
    type = "Cancel side:Sell";
  } else if(request.price > 0 && request.qty == 0) {
    type = "Cancel side:Buy";
  } else {
    assert(false);
  }

  os << "Request: type=" << type << " id=" << request.id << " price=" << std::abs(request.price) << " qty=" << std::abs(request.qty);

  return os.str();
}

std::ostream& operator<<(std::ostream& os, const Request& request) {
  return os << to_string(request);
}

/*
 * Reason codes for order events
 */

struct Reason {
  static constexpr int32_t Unknown = 0;
  static constexpr int32_t InvalidRequest = 1;
  static constexpr int32_t PriceBandViolation = 2;
  static constexpr int32_t PriceLevelFull = 3;
  static constexpr int32_t LevelOutdated = 4;
  static constexpr int32_t SlotOutdated = 6;
  static constexpr int32_t IOC = 7;
  static constexpr int32_t FOK = 8;
};

std::string to_string(int32_t reason) {
  switch(reason) {
  case Reason::InvalidRequest:
    return "InvalidRequest";
  case Reason::PriceBandViolation:
    return "PriceBandViolation";
  case Reason::PriceLevelFull:
    return "PriceLevelFull";
  case Reason::LevelOutdated:
    return "LevelOutdated";
  case Reason::SlotOutdated:
    return "SlotOutdated";
  case Reason::IOC:
    return "IOC";
  case Reason::FOK:
    return "FOK";
  default:
    return "UnknownReason(" + std::to_string(reason) + ")";
  }
}

/*
 * EventType
 */

enum EventType : int32_t {
  ETrade = -1,

  ECreateAccepted = -2,
  ECreateRejected = -3,

  EUpdateAccepted = -4,
  EUpdateRejected = -5,

  ECancelAccepted = -6,
  ECancelRejected = -7,

  ELevelExpired = -8,
  ELevelCreated = -9,

  EOrderFilled = -11,
};

/*
 * Event
 */

struct Event {
  int32_t m1{0};
  int32_t m2{0};
  int32_t m3{0};
  int32_t m4{0};
};

inline bool operator==(const Event& lhs, const Event& rhs) {
  bool result = true;

  result = result && (lhs.m1 == 0 || rhs.m1 == 0 || lhs.m1 == rhs.m1);
  result = result && (lhs.m2 == 0 || rhs.m2 == 0 || lhs.m2 == rhs.m2);
  result = result && (lhs.m3 == 0 || rhs.m3 == 0 || lhs.m3 == rhs.m3);
  result = result && (lhs.m4 == 0 || rhs.m4 == 0 || lhs.m4 == rhs.m4);
  return result;
}

inline Event Trade(int32_t price, int32_t qty, int32_t maker, int32_t taker) {
  return {.m1 = price, .m2 = qty, .m3 = maker, .m4 = taker};
}

inline Event CreateAccepted(int32_t id, int32_t slot, int32_t qty) {
  return {.m1 = EventType::ECreateAccepted, .m2 = id, .m3 = slot, .m4 = qty};
}

inline Event CreateRejected(int32_t id, int32_t size, int32_t reason = Reason::InvalidRequest) {
  return {.m1 = EventType::ECreateRejected, .m2 = id, .m3 = size, .m4 = reason};
}

inline Event CancelAccepted(int32_t id) {
  return {.m1 = EventType::ECancelAccepted, .m2 = id};
}

inline Event CancelRejected(int32_t id, int32_t reason = Reason::InvalidRequest) {
  return {.m1 = EventType::ECancelRejected, .m2 = id, .m3 = reason};
}

inline Event UpdateAccepted(int32_t id) {
  return {.m1 = EventType::EUpdateAccepted, .m2 = id};
}

inline Event UpdateRejected(int32_t id, int32_t size, int32_t reason = Reason::InvalidRequest) {
  return {.m1 = EventType::EUpdateRejected, .m2 = id, .m3 = size, .m4 = reason};
}

inline Event LevelExpired(int32_t price, int32_t id) {
  return {.m1 = EventType::ELevelExpired, .m2 = price, .m3 = id};
}

inline Event LevelsCreated(int32_t fromPrice, int32_t toPrice) {
  return {.m1 = EventType::ELevelCreated, .m2 = fromPrice, .m3 = toPrice};
}

inline Event OrderFilled(int32_t id) {
  return {.m1 = EventType::EOrderFilled, .m2 = 0, .m3 = id, .m4 = 0};
}

inline std::string to_string(const Event& event) {
  std::ostringstream os;

  if(event.m1 > 0) {
    os << "Trade: price=" << event.m1 << " qty=" << event.m2 << " maker=" << event.m3 << " taker=" << event.m4;
  } else if(event.m1 == EventType::ECreateAccepted) {
    os << "CreateAccepted: id=" << event.m2 << " slot=" << event.m3 << " qty=" << event.m4;
  } else if(event.m1 == EventType::ECreateRejected) {
    os << "CreateRejected: id=" << event.m2 << " size=" << event.m3 << " reason=" << to_string(event.m4);
  } else if(event.m1 == EventType::EUpdateAccepted) {
    os << "UpdateAccepted: id=" << event.m2;
  } else if(event.m1 == EventType::EUpdateRejected) {
    os << "UpdateRejected: id=" << event.m2 << " size=" << event.m3 << " reason=" << to_string(event.m4);
  } else if(event.m1 == EventType::ECancelAccepted) {
    os << "CancelAccepted: id=" << event.m2;
  } else if(event.m1 == EventType::ECancelRejected) {
    os << "CancelRejected: id=" << event.m2 << " reason=" << to_string(event.m3);
  } else if(event.m1 == EventType::ELevelExpired) {
    os << "LevelExpired: price=" << event.m2 << " id=" << event.m3;
  } else if(event.m1 == EventType::ELevelCreated) {
    os << "LevelsCreated: fromPrice=" << event.m2 << " toPrice=" << event.m3;
  } else if (event.m1 == EventType::EOrderFilled) {
    os << "OrderFilled: id=" << event.m3;
  } else {
    os << "UnknownEvent: m1=" << event.m1 << " m2=" << event.m2 << " m3=" << event.m3 << " m4=" << event.m4;
  }

  return os.str();
}

inline std::ostream& operator<<(std::ostream& os, const Event& event) {
  return os << to_string(event);
}

/*
 * QueueOut
 */

struct QueueOut {
  void push(const Event& event) noexcept {
    for(int i = 0; i < 8; i++) {
      if(_queue.push(event)) {
        return;
      } else {
        _mm_pause();
      }
    }

    for(;;) {
      if(_queue.push(event)) {
        return;
      } else {
        _mm_pause();
        _mm_pause();
      }
    }
  }

  Event pop() noexcept {
    Event event;
    _queue.pop(event);
    return event;
  }

  [[nodiscard]] bool empty() const noexcept {
    return _queue._approx_empty();
  }

  std::size_t clear() noexcept {
    std::size_t count = 0;

    while(_queue._approx_empty() == false) {
      count += 1;

      Event event;
      _queue.pop(event);
    }

    return count;
  }

  std::size_t log(const std::string& prefix = "") {
    std::size_t count = 0;

    while(_queue._approx_empty() == false) {
      count += 1;

      Event event;
      _queue.pop(event);

      std::cout << prefix << event << std::endl;
    }

    return count;
  }

  RingBufferSPSC<Event, 128> _queue;
};

/*
 * Order
 */

struct Order: IntrusiveListNode<Order> {
  Order(Id id, Qty qty) noexcept
    : id(id)
    , qty(qty) //
  {
  }

  Id id;
  Qty qty;
};

/*
 * Level
 */

struct Level : IntrusiveTreapNode<Level> {

public: /* ctor, dtor */

  using key_type = Price;

  Level(Price price, Id priority = -1) noexcept
    : _price(price)
    , _priority(priority) //
  {
    if (_priority == -1) {
      _priority = _gen_priority();
    }
  }

  Level(Level&&) = delete;
  Level(const Level&) = delete;

  ~Level() noexcept {
    assert(_list.empty());
  }

  Level& operator=(Level&&) = delete;
  Level& operator=(const Level&) = delete;

public: /* api */

  Price price() const noexcept {
    return _price;
  }

  template<Side side>
  void push(Order& order) noexcept {
    _list.push_back(order);
  }

  template<Side side>
  void update(Order& order, Qty newQty) noexcept {
    order.qty = newQty;
  }

  template<Side side>
  void cancel(Order& order) noexcept {
    order.id = 0;
    _list.erase(order);
  }

  Order& front() noexcept {
    return _list.front();
  }

  void pop() noexcept {
    assert(_list.empty() == false);
    assert(_list.front().id == 0);
    assert(_list.front().qty == 0);
    _list.pop_front();
  }

  bool empty() const noexcept {
    return _list.empty();
  }

  Price _get_key() const noexcept {
    return _price;
  }

  Id _get_priority() const noexcept {
    return _priority;
  }

private: /* impl */

  static Id _gen_priority() noexcept {
    thread_local static LCG rng{7u};
    return (Id)(rng() & 0x7FFFFFFF);
  }

public: /* members */

  Price _price;
  Id _priority;

  IntrusiveList<Order> _list;
};

/*
 * MatchingEngine
 */

struct MatchingEngine {
public: /* ctor, dtor */

  explicit MatchingEngine(QueueOut& out)
    : _out(out)
    , _sellLevels()
    , _buyLevels()  //
  {
    _sellLevels.insert(*_level_pool.construct(MaxPrice + 1));
    _buyLevels.insert(*_level_pool.construct(MinPrice - 1));
  }

  MatchingEngine(MatchingEngine&&) = delete;
  MatchingEngine(const MatchingEngine&) = delete;

  ~MatchingEngine() noexcept {
    while(_sellLevels.empty() == false) {
      Level* const level = _sellLevels.front();

      while(level->empty() == false) {
        Order& order = level->front();
        order.id = 0;
        order.qty = 0;
        level->pop();
        _order_pool.destroy(&order);
      }

      _sellLevels.erase(*level);
      _level_pool.destroy(level);
    }

    while(_buyLevels.empty() == false) {
      Level* const level = _buyLevels.front();

      while(level->empty() == false) {
        Order& order = level->front();
        order.id = 0;
        order.qty = 0;
        level->pop();
        _order_pool.destroy(&order);
      }

      _buyLevels.erase(*level);
      _level_pool.destroy(level);
    }
  }

  MatchingEngine& operator=(MatchingEngine&&) = delete;
  MatchingEngine& operator=(const MatchingEngine&) = delete;

public: /* api */

  template<Side side>
  Index insert_order(Id id, Price price, Qty qty) noexcept {
#ifdef HFT_DEBUG
    if(UNLIKELY(! (_check_order_id(id) && _check_qty(qty)))) {
      return _out.push(CreateRejected(id, qty, Reason::InvalidRequest)), -1;
    }
#endif

    qty = _trade<side>(id, price, qty);

    if(qty == 0) {
      return -1;
    }

    if(_order_pool.full()) {
      return _out.push(CreateRejected(id, qty, Reason::PriceLevelFull)), -1;
    }

    Order* const order = _order_pool.construct(id, qty);
    const Index slot = _order_pool.index_of(order);
    Level* level = _get_levels<side>().find(price);

    if (UNLIKELY(level == nullptr)) {
      if (_level_pool.full()) {
        return _out.push(CreateRejected(id, qty, Reason::PriceLevelFull)), -1;
      }
      
      level = _level_pool.construct(price);
      _get_levels<side>().insert(*level);
    }

    level->template push<side>(*order);
    _out.push(CreateAccepted(id, slot, qty));

    return slot;
  }

  template<Side side>
  void insert_order_ioc(Id id, Price price, Qty qty) noexcept {
#ifdef HFT_DEBUG
    if(UNLIKELY(! (_check_order_id(id) && _check_qty(qty)))) {
      return _out.push(CreateRejected(id, qty, Reason::InvalidRequest));
    }
#endif

    qty = _trade<side>(id, price, qty);

    if(UNLIKELY(qty != 0)) {
      _out.push(CreateRejected(id, qty, Reason::IOC));
    }
  }

  template<Side side>
  void insert_mkt_order_ioc(Id id, Qty qty) noexcept {
    insert_order_ioc<side>(id, (side == Sell) ? MinPrice : MaxPrice, qty);
  }

  template<Side side>
  void update_order(Id id, Price price, Qty newQty, Index slot) noexcept {
#ifdef HFT_DEBUG
    if(UNLIKELY(! (_check_order_id(id) && _check_qty(newQty)))) {
      return _out.push(UpdateRejected(id, newQty, Reason::InvalidRequest));
    }
#endif

    Order* const order = &_order_pool[slot];

    if(UNLIKELY(order->id != id)) {
      return _out.push(UpdateRejected(id, newQty, Reason::SlotOutdated));
    }

    Level* const level = _get_levels<side>().find(price);
    level->template update<side>(*order, newQty);
    _out.push(UpdateAccepted(id));
  }

  template<Side side>
  void cancel_order(Id id, Price price, Index slot) noexcept {
#ifdef HFT_DEBUG
    if(UNLIKELY(! (_check_order_id(id)))) {
      return _out.push(CancelRejected(id, Reason::InvalidRequest));
    }
#endif

    Order* const order = &_order_pool[slot];

    if(UNLIKELY(order->id != id)) {
      return _out.push(CancelRejected(id, Reason::SlotOutdated));
    }

    Level* const level = _get_levels<side>().find(price);
    level->template cancel<side>(*order);
    _order_pool.destroy(order);

    if (level->empty()) {
      _get_levels<side>().erase(*level);
      _level_pool.destroy(level);
    }

    _out.push(CancelAccepted(id));
  }

  QueueOut& out() noexcept {
    return _out;
  }

private: /* impl */

  bool _check_order_id(Id id) const noexcept {
    return id != 0;
  }

  bool _check_qty(Qty qty) const noexcept {
    return (qty >= MinQty) && (qty <= MaxQty);
  }

  template<Side side>
  auto& _get_levels() noexcept {
    if constexpr(side == Sell) {
      return _sellLevels;
    } else {
      return _buyLevels;
    }
  }

  template<Side side>
  Qty _trade(Id id, Price priceLimit, Qty qty) noexcept {
    const auto check_price = [](Price price, Price priceLimit) noexcept -> bool {
      if constexpr(side == Sell) {
        return price >= priceLimit;
      } else {
        return price <= priceLimit;
      }
    };

    Level* level = _get_levels<-side>().front();

    do {
      const Price price = level->price();

      if(UNLIKELY(check_price(price, priceLimit) == false)) {
        break;
      }

      do {
        Order& order = level->front();
        const Qty min = std::min(qty, order.qty);

        qty -= min;
        order.qty -= min;
        _out.push(Trade(price, min, order.id, id));

        if(order.qty == 0) {
          order.id = 0;
          order.qty = 0;
          level->pop();
          _order_pool.destroy(&order);

          if(UNLIKELY(level->empty())) {
            Level* const nextLevel = level->_next();
            _get_levels<-side>().erase(*level);
            _level_pool.destroy(level);
            level = nextLevel;

            break;
          }
        }
      } while(qty != 0);
    } while(qty != 0);

    return qty;
  }

private: /* members */

  QueueOut& _out;

  ObjectPool<Order> _order_pool{32 * 1024}; 
  ObjectPool<Level> _level_pool{32 * 1024};

  IntrusiveCTreap<Level, std::less<Price>> _sellLevels;
  IntrusiveCTreap<Level, std::greater<Price>> _buyLevels;
};

} // namespace v3
