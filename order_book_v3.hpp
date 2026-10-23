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
  Order(Id id, Price prc, Qty leaves, Qty cum, PriceLevel* lvl) noexcept
    : id(id)
    , price(prc)
    , cumQty(cum)
    , leavesQty(leaves)
    , level(lvl) //
  {
  }

  /* int64_t */ Id id;
  /* int32_t */ Price price;
  /* int32_t */ Qty cumQty;
  /* int32_t */ Qty leavesQty;
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
    assert(_orders.front().leavesQty == 0);

    _orders.pop_front();
  }

  bool empty() const noexcept {
    return _orders.empty();
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
 * OrderBook
 */

template<typename TEventHandler>
struct OrderBook {

  using EventHandler = TEventHandler;

public: /* ctor, dtor */

  explicit OrderBook(TEventHandler& events, Price centerPrice = -1)
    : _events(events)
    , _sellLevels()
    , _buyLevels()  //
  {
    /*center price not supported */ (void)centerPrice;

    _orderSentinel = _orderPool.construct(/* id */ 0, /* price */ 0, /* leavesQty */ 0, /* cumQty */ 0, /* level */ nullptr);
    
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
        ((volatile Order&)(order)).id = 0;
        ((volatile Order&)(order)).cumQty = 0;
        ((volatile Order&)(order)).leavesQty = 0;
        ((volatile Order&)(order)).level = nullptr;
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
        ((volatile Order&)(order)).id = 0;
        ((volatile Order&)(order)).cumQty = 0;
        ((volatile Order&)(order)).leavesQty = 0;
        ((volatile Order&)(order)).level = nullptr;
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
  Index insert_order(Id id, Price price, Qty qty) noexcept {
    const Qty leavesQty = _trade<side>(id, price, qty);
    
    if(leavesQty == 0) {
      return -1;
    }
    
    PriceLevel* const level = _get_or_create_level<side>(price);
    Order* const order = _orderPool.construct(id, price, leavesQty, qty - leavesQty, level);
    level->template push<side>(*order);
    const Index slot = _orderPool.index_of(order);

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

    PriceLevel* const level = order->level;
    assert(level != nullptr);
    level->template cancel<side>(*order);
    const Qty cumQty = order->cumQty;
    
    // dead-store elimination when Order has a trivial destructor
    ((volatile Order*)(order))->id = 0;
    ((volatile Order*)(order))->cumQty = 0;
    ((volatile Order*)(order))->leavesQty = 0;
    ((volatile Order*)(order))->level = nullptr;
    _orderPool.destroy(order);

    if (level->empty()) {
      _get_levels<side>().erase(*level);
      _levelPool.destroy(level);
    }

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
  Qty _trade(Id id, Price price, Qty qty) noexcept {
    const auto check_price = [](Price price, Price priceLimit) noexcept -> bool {
      if constexpr(side == Sell) {
        return price >= priceLimit;
      } else {
        return price <= priceLimit;
      }
    };

    Qty cumQty = 0;
    Qty leavesQty = qty;
    PriceLevel* level = _get_levels<-side>().front();

    do {
      const Price lastPrice = level->price();

      if(UNLIKELY(check_price(lastPrice, price) == false)) {
        break;
      }

      do {
        Order& order = level->front();
        const Qty lastQty = std::min(leavesQty, order.leavesQty);

        cumQty += lastQty;
        leavesQty -= lastQty;
        order.cumQty += lastQty;
        order.leavesQty -= lastQty;

        _events.onTrade(
            /*  taker */       id, -1,  side,       price,       cumQty,       leavesQty,
            /*  maker */ order.id, -1, -side, order.price, order.cumQty, order.leavesQty,
            /* common */ lastPrice, lastQty
        );

        if(order.leavesQty == 0) {
          ((volatile Order&)(order)).id = 0;
          ((volatile Order&)(order)).cumQty = 0;
          ((volatile Order&)(order)).leavesQty = 0;
          ((volatile Order&)(order)).level = nullptr;
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
      } while(leavesQty != 0);
    } while(leavesQty != 0);

    return leavesQty;
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