/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <random>
#include <set>
#include <unordered_map>
#include <vector>

#include "command.hpp"
#include "common.hpp"
#include "event.hpp"
#include "random.hpp"
#include "timer.hpp"


class CommandGenerator {
public: /* ctor, dtor */

  CommandGenerator() = default;

public: /* api */

  std::vector<Command> generate(
      Price startPrice,
      Price endPrice,
      int32_t iters,
      double probMarket,
      double probCancel,
      double probBuy = 0.5) //
  {
    std::set<int32_t> activeIds;
    std::vector<Command> out(iters);
    std::unordered_map<int32_t, Price> idPrice;
    std::unordered_map<int32_t, int32_t> idSide;

    idPrice.reserve(std::max(0, iters));
    idSide.reserve(std::max(0, iters));

    int32_t nextId = 1;
    const int32_t lo = std::min(startPrice, endPrice);
    const int32_t hi = std::max(startPrice, endPrice);
    const double step = (iters > 0) ? (1.0 * (endPrice - startPrice) / iters) : 0.0;
    const double scale = 1.0;

    for(int32_t i = 0; i < iters; i++) {
      auto& e = out[i];
      e.seq = i;
      const double u = _uni01(_rng);

      const bool isCancel = (u < probCancel) && ! activeIds.empty();
      const bool isMarket = ! isCancel && (u < probCancel + probMarket);

      if(isCancel) {
        const int32_t probeId = 1 + int32_t(_uni01(_rng) * (nextId - 1));
        auto it = activeIds.lower_bound(probeId);

        if(it == activeIds.end()) {
          it = activeIds.begin();
        }

        e.order = *it;
        e.price = idPrice[e.order] * idSide[e.order];
        e.qty = 0;

        activeIds.erase(it);
        idPrice.erase(e.order);
        idSide.erase(e.order);
      } else if(isMarket) {
        e.order = nextId++;
        e.price = 0;
        e.qty = 1 + int32_t(_uni01(_rng) * 10);

        if(_uni01(_rng) > probBuy) {
          e.qty = -e.qty;
        }
      } else /* Limit Order */ {
        e.order = nextId++;
        e.qty = 1 + int32_t(_uni01(_rng) * 10);

        if(_uni01(_rng) > probBuy) {
          e.qty = -e.qty;
        }

        const double trendPrice = startPrice + (step * i);
        const double s = _uniSigned(_rng);
        const double laplaceNoise = -scale * ((s < 0.0) ? -1.0 : 1.0) * std::log(1.0 - 2.0 * std::abs(s));

        const int32_t candidate = int32_t(std::round(trendPrice + laplaceNoise));
        e.price = std::max(lo, std::min(candidate, hi));

        idPrice[e.order] = e.price;
        idSide[e.order] = (e.qty > 0) ? 1 : -1;
        activeIds.insert(e.order);
      }
    }

    return out;
  }

private: /* members */

  std::mt19937 _rng{12345};
  std::uniform_real_distribution<double> _uni01{0.0, 1.0};
  std::uniform_real_distribution<double> _uniSigned{-0.5, 0.5};
};

template<typename OBook, Side makerSide>
void test_insert(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  oBook.template insert_order<makerSide>(1, price, 100);
  Event event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.makerId == 1);
  assert(event.makerSide == makerSide);
  assert(event.makerPrice == price);
  assert(event.makerLeavesQty == 100);

  oBook.template insert_order<makerSide>(2, price, 100);
  event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.makerId == 2);
  assert(event.makerSide == makerSide);
  assert(event.makerPrice == price);
  assert(event.makerLeavesQty == 100);
}

template<typename OBook, Side makerSide>
void test_delete(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  const Index slot = oBook.template insert_order<makerSide>(1, price, 100);
  Event event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.makerId == 1);

  oBook.template cancel_order<makerSide>(1, slot, price);
  event = oBook.eh().pop();
  assert(event.type == ORDER_CANCELED);
  assert(event.makerId == 1);
}

template<typename OBook, Side makerSide>
void test_double_delete(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  const Index slot = oBook.template insert_order<makerSide>(1, price, 100);
  Event event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.makerId == 1);

  oBook.template cancel_order<makerSide>(1, slot, price);
  event = oBook.eh().pop();
  assert(event.type == ORDER_CANCELED);
  assert(event.makerId == 1);

  oBook.template cancel_order<makerSide>(1, slot, price);
  event = oBook.eh().pop();
  assert(event.type == CANCEL_REJECTED);
  assert(event.makerId == 1);
}

template<typename OBook, Side makerSide>
void test_trade_across_levels(Price centerPrice) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, centerPrice);

  const Price firstPrice = centerPrice - makerSide;
  const Price secondPrice = centerPrice - 2 * makerSide;
  const Price takerPrice = centerPrice - 3 * makerSide;

  oBook.template insert_order<makerSide>(1, firstPrice, 20);
  Event event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);

  oBook.template insert_order<makerSide>(2, secondPrice, 30);
  event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);

  const Index slot = oBook.template insert_order<-makerSide>(3, takerPrice, 35);
  assert(slot == -1);

  event = oBook.eh().pop();
  assert(event.type == TRADE);
  assert(event.takerId == 3);
  assert(event.takerSide == -makerSide);
  assert(event.takerPrice == takerPrice);
  assert(event.takerCumQty == 20);
  assert(event.takerLeavesQty == 15);
  assert(event.makerId == 1);
  assert(event.makerPrice == firstPrice);
  assert(event.makerCumQty == 20);
  assert(event.makerLeavesQty == 0);
  assert(event.lastPrice == firstPrice);
  assert(event.lastQty == 20);

  event = oBook.eh().pop();
  assert(event.type == TRADE);
  assert(event.takerCumQty == 35);
  assert(event.takerLeavesQty == 0);
  assert(event.makerId == 2);
  assert(event.makerPrice == secondPrice);
  assert(event.makerCumQty == 15);
  assert(event.makerLeavesQty == 15);
  assert(event.lastPrice == secondPrice);
  assert(event.lastQty == 15);
}

template<typename OBook, Side makerSide>
void test_cancel_after_partial_trade(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  const Index slot = oBook.template insert_order<makerSide>(1, price, 100);
  Event event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);

  const Index takerSlot = oBook.template insert_order<-makerSide>(2, price, 25);
  assert(takerSlot == -1);
  event = oBook.eh().pop();
  assert(event.type == TRADE);
  assert(event.makerCumQty == 25);
  assert(event.makerLeavesQty == 75);

  oBook.template cancel_order<makerSide>(1, slot, price);
  event = oBook.eh().pop();
  assert(event.type == ORDER_CANCELED);
  assert(event.makerCumQty == 25);
}

template<typename OBook, Side makerSide>
void test_trade(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);
  Event event;

  oBook.template insert_order<makerSide>(1, price, 100);
  event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.makerId == 1);
  assert(event.makerSide == makerSide);
  assert(event.makerPrice == price);
  assert(event.makerLeavesQty == 100);

  oBook.template insert_order<-makerSide>(2, price, 100);
  event = oBook.eh().pop();
  assert(event.type == TRADE);
  assert(event.makerId == 1);
  assert(event.takerId == 2);
  assert(event.lastPrice == price);
  assert(event.lastQty == 100);
}

template<typename OBook, Side makerSide>
void test_trade_level(Price price, Price tradePrice) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  const Qty qty = 100;
  const Index orders = 8;

  for(Index order = 1; order <= orders; order++) {
    oBook.template insert_order<makerSide>(order, tradePrice, qty);
    Event event = oBook.eh().pop();
    assert(event.type == ORDER_ACK);
    assert(event.makerId == order);
  }

  oBook.template insert_order<-makerSide>(orders + 1, tradePrice, orders * qty);

  for(Index i = 1; i <= 8; i++) {
    Event event = oBook.eh().pop();
    assert(event.type == TRADE);
    assert(event.makerId == i);
    assert(event.takerId == orders + 1);
  }
}

template<typename OBook, Side makerSide>
void test_trade_level(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  for(int32_t price = 1; price <= 128; price++) {
    test_trade_level<OBook, makerSide>(price, price);
  }
}

template<typename OBook>
void test_trend(int32_t price, int32_t trend = 1) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  const Price minPrice = 1;
  const Price maxPrice = 128;

  const auto update_price = [&](Price price, Price trend) -> Price {
    price += trend;

    price = std::max(price, MinPrice);
    price = std::min(price, MaxPrice);

    return price;
  };

  for(int32_t iter = 0; iter != 3 * (maxPrice - minPrice); iter += 1, price = update_price(price, trend)) {
    oBook.template insert_order<Sell>(1000000 + price, price, 100);
    oBook.eh().clear();

    oBook.template insert_order<Buy>(2000000 + price, price, 100);
    oBook.eh().clear();
  }
}

template<typename OBook>
void benchmark(int32_t iters) {
  typename OBook::EventHandler eh;
  OBook oBook(eh);

  struct Benchmark {
    Benchmark(OBook& oBook, int32_t iters)
      : _iters(iters)
      , _engine(oBook)  //
    {
    }

    int32_t _iters;
    OBook& _engine;
    std::vector<int> _slots;
    std::vector<Command> _requests;

    void setup() {
      CommandGenerator gen;
      std::vector<Command> orders;

      _slots.resize(_iters + 10);
      _slots.assign(_iters + 10, -1);

      orders = gen.generate(1, 1000, _iters / 2, /* pMarket */ 0.1, /* pCancel */ 0.25);
      _requests.insert(_requests.end(), orders.begin(), orders.end());

      orders = gen.generate(1000, 1, _iters / 2, /* pMarket */ 0.1, /* pCancel */ 0.25);
      _requests.insert(_requests.end(), orders.begin(), orders.end());
    }

    void run() {

      for(const auto& e : _requests) {
        if(e.qty > 0) {
          if(e.price == 0) {
            _engine.template insert_mkt_order<Buy>(e.order, e.qty);
          } else {
            _slots[e.order] = _engine.template insert_order<Buy>(e.order, e.price, e.qty);
          }
        } else if(e.qty < 0) {
          if(e.price == 0) {
            _engine.template insert_mkt_order<Sell>(e.order, -e.qty);
          } else {
            _slots[e.order] = _engine.template insert_order<Sell>(e.order, e.price, -e.qty);
          }
        } else {
          if((e.price > 0) && (_slots[e.order] != -1)) {
            _engine.template cancel_order<Buy>(e.order, _slots[e.order], e.price);
          } else if((e.price < 0) && (_slots[e.order] != -1)) {
            _engine.template cancel_order<Sell>(e.order, _slots[e.order], -e.price);
          }
        }

#ifndef NDEBUG
        _engine.eh().log(to_string(e) + " => ");
#else
        _engine.eh().clear();
#endif
      }
    }

    void teardown() {
    }

  } bench(oBook, iters);

  Timer<1>(bench).log([iters](long int ns, long int cyc) {
    const std::string& log = std::format(
        "{} :: {} iters :: {} iter/s :: "
        "{:.1f} ns/iter :: "
        "{:.1f} cyc/iter\n",
        PROFILE,
        iters,
        (int)(1e9 * iters / ns),
        (1.0 * ns / iters),
        (1.0 * cyc / iters));

    std::cout << log << std::flush;
  });
}
