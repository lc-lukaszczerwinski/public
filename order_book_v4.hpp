/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cassert>
#include <cstdint>

#include "command.hpp"
#include "common.hpp"
#include "event.hpp"
#include "intrusive_list.hpp"
#include "intrusive_treap.hpp"
#include "object_pool.hpp"
#include "open_hash.hpp"
#include "random.hpp"

namespace v4 {

/*
 * Order
 */

struct Order: IntrusiveListNode<Order> {
  Order(Id id, Price prc, Qty qty) noexcept
    : id(id)
    , price(prc)
    , leavesQty(qty) //
  {
  }

  Id id;
  Price price;
  Qty leavesQty;
};

/*
 * PriceLevel
 */

struct PriceLevel: IntrusiveTreapNode<PriceLevel> {

public: /* ctor, dtor */

  using key_type = Price;

  PriceLevel(Price price, Id priority = -1) noexcept
    : _price(price)
    , _priority(priority) //
  {
    if (_priority == -1) {
      _priority = _gen_priority();
    }
  }

  PriceLevel(PriceLevel&&) = delete;
  PriceLevel(const PriceLevel&) = delete;

  ~PriceLevel() noexcept {
    _orders.clear();
  }

  PriceLevel& operator=(PriceLevel&&) = delete;
  PriceLevel& operator=(const PriceLevel&) = delete;

public: /* api */

  Price price() const noexcept {
    return _price;
  }

  template<Side side>
  void push(Order& order) noexcept {
    _orders.push_back(order);
  }

  template<Side side>
  void cancel(Order& order) noexcept {
    _orders.erase(order);
  }

  Order& front() noexcept {
    return _orders.front();
  }

  void pop() noexcept {
    assert(_orders.empty() == false);
    assert(_orders.front().id == 0);
    assert(_orders.front().leavesQty == 0);
    _orders.pop_front();
  }

  bool empty() const noexcept {
    return _orders.empty();
  }

  /* extension: slow */ Qty _ext_depth_at() const noexcept {
    Qty qty = 0;

    for (const Order* node = &_orders.front(); node != nullptr; node = node->_M_next) {
      qty += node->leavesQty;
    }

    return qty;
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

  IntrusiveList<Order> _orders;
};

/*
 *
 */

struct IEventHandler {
  /* deprecated */
  void orderACK(Id seq, Id id, Side side, Price price, Qty qty) {
  }

  void modifyACK(Id seq, Id id, Side side, Price newPrice, Qty newQty) {
  }

  void modifyReject(Id seq, Id id) {
  }

  void cancelACK(Id seq, Id id, Side side, Price price, Qty qty) {
  }

  void cancelReject(Id seq, Id id) {
  }

  void trade(Id seq, Id maker, Id taker, Price price, Qty qty) {
  }
  void onOrderTraded(Id clOrdId, Index slot, Side side, Price orderPrice, Price lastPrice, Qty lastQty, Qty cumQty, Qty leavesQty) {
  }
};

/*
 * OrderBook
 */

template<typename TEventHandler>
struct OrderBook {

  using EventHandler = TEventHandler;

public: /* ctor, dtor */

  explicit OrderBook(TEventHandler& events, Price centerPrice = -1)
    : _events(events)
    , _sellLevels()
    , _buyLevels() //
  {
    /*center price not supported */ (void)centerPrice;

    _orderSentinel = _orderPool.construct(/* id */ 0, /* price */ 0, /* qty */ 0);

    _sellLevels.insert(*_levelPool.construct(MaxPrice + 1));
    _buyLevels.insert(*_levelPool.construct(MinPrice - 1));
  }

  OrderBook(OrderBook&&) = delete;
  OrderBook(const OrderBook&) = delete;

  ~OrderBook() noexcept {
    while (_sellLevels.empty() == false) {
      PriceLevel* const level = _sellLevels.front();

      while (level->empty() == false) {
        Order& order = level->front();
        order.id = 0;
        order.leavesQty = 0;
        level->pop();
        _orderMap.erase(order.id);
        _orderPool.destroy(&order);
      }

      _sellLevels.erase(*level);
      _levelPool.destroy(level);
    }

    while (_buyLevels.empty() == false) {
      PriceLevel* const level = _buyLevels.front();

      while (level->empty() == false) {
        Order& order = level->front();
        order.id = 0;
        order.leavesQty = 0;
        level->pop();
        _orderMap.erase(order.id);
        _orderPool.destroy(&order);
      }

      _buyLevels.erase(*level);
      _levelPool.destroy(level);
    }

    _orderPool.destroy(_orderSentinel);
  }

  OrderBook& operator=(OrderBook&&) = delete;
  OrderBook& operator=(const OrderBook&) = delete;

public: /* api */

  TEventHandler& eh() noexcept {
    return _events;
  }

  Price best_bid() const noexcept {
    assert(_buyLevels.empty() == false);
    return _buyLevels.front()->_get_key();
  }

  Price best_ask() const noexcept {
    assert(_sellLevels.empty() == false);
    return _sellLevels.front()->_get_key();
  }

  template<Side side>
  Index insert_order(Id id, Price price, Qty qty, Id seq = -1) noexcept {
    const Qty leavesQty = _trade<side>(id, price, qty, seq);

    if (leavesQty == 0) {
      return -1;
    }

    if (leavesQty == qty) {
      _events.orderACK(seq, id, side, price, qty);
    }

    Order* const order = _orderPool.construct(id, price, leavesQty);
    _orderMap.insert(id, order);

    PriceLevel* const level = _get_or_create_level<side>(price);
    level->template push<side>(*order);

    return 0;
  }

  template<Side side>
  void insert_ioc_order(Id id, Price price, Qty qty, Id seq = -1) noexcept {
    qty = _trade<side>(id, price, qty, seq);

    if (UNLIKELY(qty != 0)) {
      _events.cancelACK(seq, id, side, price, qty);
    }
  }

  template<Side side>
  void insert_mkt_order(Id id, Qty qty, Id seq = -1) noexcept {
    insert_ioc_order<side>(id, (side == Sell) ? MinPrice : MaxPrice, qty, seq);
  }

  template<Side side>
  void cancel_order(Id id, Price price, Index /* slot */, Id seq = -1) noexcept {
    Order* const order = _orderMap.findOrDefault(id, _orderSentinel);

    if (UNLIKELY(order->id != id)) {
      _events.cancelReject(seq, id);
      return;
    }

    if (UNLIKELY(order->price != price)) {
      _events.cancelReject(seq, id);
      return;
    }

    PriceLevel* const level = _get_levels<side>().find(price);
    level->template cancel<side>(*order);

    const Qty qty = order->leavesQty;
    order->id = 0;
    order->leavesQty = 0;
    _orderPool.destroy(order);
    _orderMap.erase(id);

    if (level->empty()) {
      _get_levels<side>().erase(*level);
      _levelPool.destroy(level);
    }

    _events.cancelACK(seq, id, side, price, qty);
  }

  template<Side side>
  /* extension: slow */ Qty _ext_depth_at(Price price) noexcept {
    PriceLevel* level = _get_levels<side>().find(price);

    if (UNLIKELY(level == nullptr)) {
      return 0;
    }

    return level->_ext_depth_at();
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
    if constexpr (side == Sell) {
      return _sellLevels;
    } else {
      return _buyLevels;
    }
  }

  template<Side side>
  PriceLevel* _get_or_create_level(Price price) noexcept {
    PriceLevel* level = _get_levels<side>().find(price);

    if (UNLIKELY(level == nullptr)) {
      level = _levelPool.construct(price);
      _get_levels<side>().insert(*level);
    }

    return level;
  }

  template<Side side>
  Qty _trade(Id id, Price price, Qty leavesQty, Id seq) noexcept {
    const auto check_price = [](Price lastPrice, Price price) noexcept -> bool {
      if constexpr (side == Sell) {
        return lastPrice >= price;
      } else {
        return lastPrice <= price;
      }
    };

    PriceLevel* level = _get_levels<-side>().front();

    do {
      const Price lastPrice = level->price();

      if (UNLIKELY(check_price(lastPrice, price) == false)) {
        break;
      }

      do {
        Order& order = level->front();
        const Qty lastQty = std::min(leavesQty, order.leavesQty);

        leavesQty -= lastQty;
        order.leavesQty -= lastQty;

        /* old: text */ _events.trade(seq, /* maker */ order.id, /* taker */ id, /* price */ lastPrice, /* qty */ lastQty);
        /* new: FIX  */ _events.onOrderTraded(id, -1, side, price, lastPrice, lastQty, -1, leavesQty);
        /* new: FIX  */ _events.onOrderTraded(order.id, -1, -side, order.price, lastPrice, lastQty, -1, order.leavesQty);

        if (order.leavesQty == 0) {
          order.id = 0;
          order.leavesQty = 0;
          level->pop();
          _orderMap.erase(order.id);
          _orderPool.destroy(&order);

          if (UNLIKELY(level->empty())) {
            PriceLevel* const nextLevel = level->_next();
            _get_levels<-side>().erase(*level);
            _levelPool.destroy(level);
            level = nextLevel;

            break;
          }
        }
      } while (leavesQty != 0);
    } while (leavesQty != 0);

    return leavesQty;
  }

private: /* members */

  TEventHandler& _events;

  Order* _orderSentinel{nullptr};
  ObjectPool<Order> _orderPool;
  ObjectPool<PriceLevel> _levelPool;
  OpenHash<Id, Order*> _orderMap{1 * 1024 * 1024};

  IntrusiveCTreap<PriceLevel, std::less<Price>> _sellLevels;
  IntrusiveCTreap<PriceLevel, std::greater<Price>> _buyLevels;
};

} // namespace v4
