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
#include "object_pool.hpp"

namespace v2 {

/*
 * Order
 */

struct Order: IntrusiveListNode<Order> {
  Order(Id id, Price prc, Qty leaves, Qty cum = 0) noexcept
    : id(id)
    , price(prc)
    , cumQty(cum)
    , leavesQty(leaves) //
  {
  }

  /* int64_t */ Id id;
  /* int32_t */ Price price;
  /* int32_t */ Qty cumQty;
  /* int32_t */ Qty leavesQty;
};

/*
 * PriceLevel
 */

struct PriceLevel {

public: /* ctor, dtor */

  PriceLevel() noexcept = default;

  PriceLevel(PriceLevel&&) = delete;
  PriceLevel(const PriceLevel&) = delete;

  ~PriceLevel() {
    _orders.clear();
  }

  PriceLevel& operator=(PriceLevel&&) = delete;
  PriceLevel& operator=(const PriceLevel&) = delete;

public: /* api */

  template<Side side>
  void push(Order& order) noexcept {
    _orders.push_back(order);
  }

  template<Side side>
  void cancel(Order& order) noexcept {
    order.id = 0;
    order.cumQty = 0;
    order.leavesQty = 0;
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

private: /* members */

  IntrusiveList<Order> _orders;
};

/*
 * OrderBook
 */

template<typename TEventHandler>
struct OrderBook {

  using EventHandler = TEventHandler;
  static constexpr Index Capacity = 2 * 1024;
  static constexpr Index Levels = Capacity / 2 - 1;

public: /* ctor, dtor */

  explicit OrderBook(TEventHandler& events, Price centerPrice = 1000)
    : _events(events)
  {
    _minPrice = std::max(centerPrice - Levels, MinPrice);
    _maxPrice = std::min(_minPrice + Levels + Levels, MaxPrice);

    _topSellPrice = _maxPrice;
    _topBuyPrice = _minPrice;
  }

  OrderBook(OrderBook&&) = delete;
  OrderBook(const OrderBook&) = delete;

  OrderBook& operator=(OrderBook&&) = delete;
  OrderBook& operator=(const OrderBook&) = delete;

public: /* api */

  TEventHandler& eh() noexcept {
    return _events;
  }

  Price get_min_price() const noexcept {
    return _minPrice;
  }

  Price get_center_price() const noexcept {
    return (_minPrice + _maxPrice) / 2;
  }

  Price get_max_price() const noexcept {
    return _maxPrice;
  }

  template<Side side>
  Index insert_order(Id id, Price price, Qty qty) noexcept {
    if(UNLIKELY(_check_price(price) == false)) {
      return _events.orderReject(id), -1;
    }

    if(_orderPool.full()) {
      return _events.orderReject(id), -1;
    }

    const Qty leavesQty = _trade<side>(id, price, qty);
    
    if(leavesQty == 0) {
      return -1;
    }
    
    Order* order = _orderPool.construct(id, price, leavesQty);
    const Index slot = _orderPool.index_of(order);
    
    PriceLevel& level = get_level_by_price(price);
    level.template push<side>(*order);
    
    if constexpr(side == Sell) {
      if(UNLIKELY(price < _topSellPrice)) {
        _topSellPrice = price;
      }
    } else {
      if(UNLIKELY(price > _topBuyPrice)) {
        _topBuyPrice = price;
      }
    }
    
    if (leavesQty == qty) {
      _events.onOrderAccepted(id, slot, side, price, leavesQty);
    }
    
    return slot;
  }

  template<Side side>
  void insert_mkt_order(Id id, Qty qty) noexcept {
    insert_ioc_order<side>(id, (side == Sell) ? MinPrice : MaxPrice, qty);
  }

  template<Side side>
  void insert_ioc_order(Id id, Price price, Qty qty) noexcept {   
    qty = _trade<side>(id, price, qty);

    if(UNLIKELY(qty != 0)) {
      _events.onOrderCanceled(id, /* slot */ 0, side, price, qty);
    }
  }

  template<Side side>
  void cancel_order(Id id, Index slot, Price price) noexcept {
    if(UNLIKELY(slot == -1)) {
      _events.onCancelRejected(id, slot, side, price);
      return;
    }

    Order* const order = &_orderPool[slot];

    if(UNLIKELY(order->id != id)) {
      _events.onCancelRejected(id, slot, side, price);
      return;
    }

    if(UNLIKELY(order->price != price)) {
      _events.onCancelRejected(id, slot, side, price);
      return;
    }

    const Qty cumQty = order->cumQty;

    PriceLevel& level = get_level_by_price(price);
    level.template cancel<side>(*order);

    if(UNLIKELY(level.empty())) {
      if constexpr(side == Sell) {
        if(UNLIKELY(_topSellPrice == price)) {
          _topSellPrice = std::min(_topSellPrice + 1, _maxPrice);
        }
      } else {
        if(UNLIKELY(_topBuyPrice == price)) {
          _topBuyPrice = std::max(_topBuyPrice - 1, _minPrice);
        }
      }
    }

    _orderPool.destroy(order);
    _events.onOrderCanceled(id, slot, side, price, cumQty);
  }

private: /* impl */

  bool _check_order_id(Id id) const noexcept {
    return id != 0;
  }

  bool _check_qty(Qty qty) const noexcept {
    return (qty >= MinQty) && (qty <= MaxQty);
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

    const Price takerPrice = priceLimit;
    priceLimit = get_bottom_price<side>(priceLimit);

    Qty cumQty = 0;
    Qty leavesQty = qty;
    Price lastPrice = get_center_price();

    do {
      const Price price = get_top_price<-side>();

      if(UNLIKELY(check_price(price, priceLimit) == false)) {
        break;
      }

      PriceLevel& level = get_level_by_price(price);

      if(UNLIKELY(level.empty())) {
        set_top_price<-side>(price + side);
        continue;
      }

      do {
        Order& order = level.front();
        const Qty lastQty = std::min(leavesQty, order.leavesQty);

        cumQty += lastQty;
        leavesQty -= lastQty;
        order.cumQty += lastQty;
        order.leavesQty -= lastQty;
        lastPrice = price;

        _events.onTrade(
            /*  taker */       id, -1,  side, takerPrice,       cumQty,       leavesQty,
            /*  maker */ order.id, -1, -side, order.price, order.cumQty, order.leavesQty,
            /* common */ lastPrice, lastQty
        );

        if(order.leavesQty == 0) {
          ((volatile Order&)(order)).id = 0;
          ((volatile Order&)(order)).cumQty = 0;
          ((volatile Order&)(order)).leavesQty = 0;

          level.pop();
          _orderPool.destroy(&order);

          if(UNLIKELY(level.empty())) {
            set_top_price<-side>(price + side);
            break;
          }
        }
      } while(leavesQty != 0);
    } while(leavesQty != 0);

    shift(lastPrice, _orderPool);
    return leavesQty;
  }

  template<Side side>
  Price get_bottom_price(Price price = (side == Sell ? MinPrice : MaxPrice)) const noexcept {
    if constexpr(side == Sell) {
      return std::max(price, _minPrice);
    } else {
      return std::min(price, _maxPrice);
    }
  }

  template<Side side>
  Price get_top_price() const noexcept {
    if constexpr(side == Sell) {
      return _topSellPrice;
    } else {
      return _topBuyPrice;
    }
  }

  template<Side side>
  void set_top_price(Price price) noexcept {
    if constexpr(side == Sell) {
      _topSellPrice = price;
    } else {
      _topBuyPrice = price;
    }
  }

  PriceLevel& get_level_by_price(Price price) noexcept {
    assert(_check_price(price));
    return get_level_by_index(price & (Capacity - 1));
  }

  PriceLevel& get_level_by_index(Index index) noexcept {
    return _levels[index & (Capacity - 1)];
  }

  void shift(Price lastPrice, ObjectFixedPool<Order>& orderPool) noexcept {
    if((lastPrice > _maxPrice - Levels / 2) && (_maxPrice < MaxPrice)) {
      _shift_up(orderPool);
    }

    if((lastPrice < _minPrice + Levels / 2) && (_minPrice > MinPrice)) {
      _shift_down(orderPool);
    }
  }

  bool _check_price(Price price) const noexcept {
    return (price >= _minPrice) && (price <= _maxPrice);
  }

private:

  template<Side side>
  void _expire_level(Price price, ObjectFixedPool<Order>& pool) noexcept {
    PriceLevel& level = get_level_by_price(price);

    while(level.empty() == false) {
      Order& order = level.front();
      _events.levelRemove(price);
      
      ((volatile Order*)this)->id = 0;
      ((volatile Order*)this)->cumQty = 0;
      ((volatile Order*)this)->leavesQty = 0;

      level.pop();
      pool.destroy(&order);
    }
  }

  void _create_level(Price price) noexcept {
    _events.levelCreate(price);
  }

  void _shift_up(ObjectFixedPool<Order>& orderPool) noexcept {
    _expire_level<Buy>(_minPrice, orderPool);
    _minPrice += 1;
    _maxPrice += 1;
    _topSellPrice = std::max(_topSellPrice, _minPrice);
    _topBuyPrice = std::max(_topBuyPrice, _minPrice);
    _create_level(_maxPrice);

    assert((_topSellPrice >= _minPrice) && (_topSellPrice <= _maxPrice));
    assert((_topBuyPrice >= _minPrice) && (_topBuyPrice <= _maxPrice));
  }

  void _shift_down(ObjectFixedPool<Order>& orderPool) noexcept {
    _expire_level<Sell>(_maxPrice, orderPool);
    _minPrice -= 1;
    _maxPrice -= 1;
    _topSellPrice = std::min(_topSellPrice, _maxPrice);
    _topBuyPrice = std::min(_topBuyPrice, _maxPrice);
    _create_level(_minPrice);

    assert((_topSellPrice >= _minPrice) && (_topSellPrice <= _maxPrice));
    assert((_topBuyPrice >= _minPrice) && (_topBuyPrice <= _maxPrice));
  }

private:

  TEventHandler& _events;

  Price _minPrice;
  Price _maxPrice;

  Price _topSellPrice;
  Price _topBuyPrice;

  ObjectFixedPool<Order> _orderPool{32 * Capacity};
  PriceLevel _levels[Capacity];
};

/*
 * CTAD
 */

template<typename TEventHandler>
OrderBook(TEventHandler&) -> OrderBook<TEventHandler>;

} // namespace v2
