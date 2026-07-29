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
#include "random.hpp"

namespace v3 {

/*
 * PriceLevel FWD
 */

struct PriceLevel;

/*
 * Order
 */

struct Order: IntrusiveListNode<Order> {
  Order(Id id, Price prc, Qty qty, PriceLevel* lvl) noexcept
    : id(id)
    , price(prc)
    , qty(qty) 
    , level(lvl) //
  {
  }

  Id id;
  Price price;
  Qty qty;
  PriceLevel* level;
};

/*
 * PriceLevel
 */

struct PriceLevel : IntrusiveTreapNode<PriceLevel> {

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
    assert(_orders.front().qty == 0);
    _orders.pop_front();
  }

  bool empty() const noexcept {
    return _orders.empty();
  }

  /* extension: slow */ Qty _ext_depth_at() const noexcept {
    Qty qty = 0;

    for(const Order* node = &_orders.front(); node != nullptr; node = node->_M_next) {
      qty += node->qty;
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
 * IEventHandler
 */

struct IEventHandler {
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
};

/*
 * OrderBook
 */

template<typename TEventHandler>
struct OrderBook {

  using EventHandler = TEventHandler;

public: /* ctor, dtor */

  explicit OrderBook(TEventHandler& events)
    : _events(events)
    , _sellLevels()
    , _buyLevels()  //
  {
    _orderSentinel = _orderPool.construct(/* id */ 0, /* price */ 0, /* qty */ 0, /* level */ nullptr);

    _sellLevels.insert(*_levelPool.construct(MaxPrice + 1));
    _buyLevels.insert(*_levelPool.construct(MinPrice - 1));
  }

  OrderBook(OrderBook&&) = delete;
  OrderBook(const OrderBook&) = delete;

  ~OrderBook() noexcept {
    while(_sellLevels.empty() == false) {
      PriceLevel* const level = _sellLevels.front();

      while(level->empty() == false) {
        Order& order = level->front();
        order.id = 0;
        order.qty = 0;
        order.level = nullptr;
        level->pop();
        _orderPool.destroy(&order);
      }

      _sellLevels.erase(*level);
      _levelPool.destroy(level);
    }

    while(_buyLevels.empty() == false) {
      PriceLevel* const level = _buyLevels.front();

      while(level->empty() == false) {
        Order& order = level->front();
        order.id = 0;
        order.qty = 0;
        order.level = nullptr;
        level->pop();
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
    _events.orderACK(seq, id, side, price, qty);
    return _insert_order<side>(id, price, qty, seq);
  }

  template<Side side>
  void insert_mkt_order(Id id, Qty qty, Id seq = -1) noexcept {
    insert_ioc_order<side>(id, (side == Sell) ? MinPrice : MaxPrice, qty, seq);
  }

  template<Side side>
  void insert_ioc_order(Id id, Price price, Qty qty, Id seq = -1) noexcept {
    _events.orderACK(seq, id, side, price, qty);
    
    qty = _trade<side>(id, price, qty, seq);

    if(UNLIKELY(qty != 0)) {
      _events.cancelACK(seq, id, side, price, qty);
    }
  }

  template<Side side>
  Index update_order(Id id, Price oldPrice, Price newPrice, Qty newQty, Index slot, Id seq = -1) noexcept { 
    _cancel_order<side>(id, oldPrice, slot, seq);
    return insert_order<side>(id, newPrice, newQty, seq);
  }

  template<Side side>
  void cancel_order(Id id, Price price, Index slot, Id seq = -1) noexcept {
    _cancel_order<side>(id, price, slot, seq);
    _events.cancelACK(seq, id, side, price, 0);
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
    if constexpr(side == Sell) {
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
  Index _insert_order(Id id, Price price, Qty qty, Id seq = -1) noexcept {
    qty = _trade<side>(id, price, qty, seq);

    if(qty == 0) {
      return -1;
    }

    PriceLevel* const level = _get_or_create_level<side>(price);
    Order* const order = _orderPool.construct(id, price, qty, level);    
    level->template push<side>(*order);
    
    return _orderPool.index_of(order);
  }

  template<Side side>
  void _cancel_order(Id id, Price price, Index slot, Id seq = -1) noexcept {
    if(UNLIKELY(slot == -1)) {
      _events.cancelReject(seq, id);
      return;
    }

    Order* const order = &_orderPool[slot];

    if(UNLIKELY(order->id != id)) {
      _events.cancelReject(seq, id);
      return;
    }

    if(UNLIKELY(order->price != price)) {
      _events.cancelReject(seq, id);
      return;
    }

    PriceLevel* const level = order->level;
    assert(level != nullptr);
    level->template cancel<side>(*order);

    // dead-store elimination when Order has a trivial destructor
    ((volatile Order*)(order))->id = 0;
    ((volatile Order*)(order))->qty = 0;
    ((volatile Order*)(order))->level = nullptr;
    _orderPool.destroy(order);

    if (level->empty()) {
      _get_levels<side>().erase(*level);
      _levelPool.destroy(level);
    }
  }

  template<Side side>
  Qty _trade(Id id, Price priceLimit, Qty qty, Id seq) noexcept {
    const auto check_price = [](Price price, Price priceLimit) noexcept -> bool {
      if constexpr(side == Sell) {
        return price >= priceLimit;
      } else {
        return price <= priceLimit;
      }
    };

    PriceLevel* level = _get_levels<-side>().front();

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
        _events.trade(seq, /* maker */ order.id, /* taker */ id, /* price */ price, /* qty */ min);

        if(order.qty == 0) {
          order.id = 0;
          order.qty = 0;
          order.level = nullptr;
          level->pop();
          _orderPool.destroy(&order);

          if(UNLIKELY(level->empty())) {
            PriceLevel* const nextLevel = level->_next();
            _get_levels<-side>().erase(*level);
            _levelPool.destroy(level);
            level = nextLevel;

            break;
          }
        }
      } while(qty != 0);
    } while(qty != 0);

    return qty;
  }

private: /* members */

  TEventHandler& _events;

  Order* _orderSentinel{nullptr};
  ObjectFixedPool<Order> _orderPool{32 * 32 * 1024}; 
  ObjectFixedPool<PriceLevel> _levelPool{32 * 1024};

  IntrusiveCTreap<PriceLevel, std::less<Price>> _sellLevels;
  IntrusiveCTreap<PriceLevel, std::greater<Price>> _buyLevels;
};

/*
 * CTAD
 */

template<typename TEventHandler>
OrderBook(TEventHandler&) -> OrderBook<TEventHandler>;

} // namespace v3
