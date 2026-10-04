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

template<typename OBook, Side side>
void test_insert(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  oBook.template insert_order<side>(1, price, 100);
  Event event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.maker == 1);
  assert(event.side == side);
  assert(event.price == price);
  assert(event.qty == 100);

  oBook.template insert_order<side>(2, price, 100);
  event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.maker == 2);
  assert(event.side == side);
  assert(event.price == price);
  assert(event.qty == 100);
}

template<typename OBook, Side side>
void test_delete(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  const Index slot = oBook.template insert_order<side>(1, price, 100);
  Event event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.maker == 1);

  oBook.template cancel_order<side>(1, price, slot);
  event = oBook.eh().pop();
  assert(event.type == CANCEL_ACK);
  assert(event.maker == 1);
}

template<typename OBook, Side side>
void test_double_delete(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  const Index slot = oBook.template insert_order<side>(1, price, 100);
  Event event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.maker == 1);

  oBook.template cancel_order<side>(1, price, slot);
  event = oBook.eh().pop();
  assert(event.type == CANCEL_ACK);
  assert(event.maker == 1);

  oBook.template cancel_order<side>(1, price, slot);
  event = oBook.eh().pop();
  assert(event.type == CANCEL_REJECT);
  assert(event.maker == 1);
}

template<typename OBook, Side side>
void test_trade(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);
  Event event;

  oBook.template insert_order<side>(1, price, 100);
  event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.maker == 1);
  assert(event.side == side);
  assert(event.price == price);
  assert(event.qty == 100);

  oBook.template insert_order<-side>(2, price, 100);
  event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.maker == 2);
  assert(event.side == -side);
  assert(event.price == price);
  assert(event.qty == 100);

  event = oBook.eh().pop();
  assert(event.type == TRADE);
  assert(event.maker == 1);
  assert(event.taker == 2);
  assert(event.price == price);
  assert(event.qty == 100);
}

template<typename OBook, Side side>
void test_trade_level(Price price, Price tradePrice) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  const Qty qty = 100;
  const Index orders = 8;

  for(Index order = 1; order <= orders; order++) {
    oBook.template insert_order<side>(order, tradePrice, qty);
    Event event = oBook.eh().pop();
    assert(event.type == ORDER_ACK);
    assert(event.maker == order);
  }

  oBook.template insert_order<-side>(orders + 1, tradePrice, orders * qty);
  Event event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.maker == orders + 1);

  for(Index i = 1; i <= 8; i++) {
    Event event = oBook.eh().pop();
    assert(event.type == TRADE);
    assert(event.maker == i);
    assert(event.taker == orders + 1);
  }
}

template<typename OBook, Side side>
void test_trade_level(Price price) {
  typename OBook::EventHandler eh;
  OBook oBook(eh, price);

  for(int32_t price = 1; price <= 128; price++) {
    test_trade_level<OBook, side>(price, price);
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
            _engine.template cancel_order<Buy>(e.order, e.price, _slots[e.order]);
          } else if((e.price < 0) && (_slots[e.order] != -1)) {
            _engine.template cancel_order<Sell>(e.order, -e.price, _slots[e.order]);
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
