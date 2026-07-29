/*
* lcv3.cpp — Adapter for LC++-v3 implementing matching_engine_api.h.
*
* Upstream: https://github.com/lc-lukaszczerwinski/public
*
* lcv3 is a lightweight, header-only matching engine with zero external
* dependencies. It uses a treap for price prioritization and intrusive FIFO
* queues for time prioritization, providing deterministic matching behavior
* and efficient order management.
*
* Features:
* - Native support for limit, market, and IOC orders
* - Native order modification handling
* - Synchronous matching on the caller thread
* - No background processing; engine_flush() is a no-op
*
* Complexity:
* - Best ask/bid price: O(1)
* - Insert: O(log N)
* - Cancel: O(1) to O(log N)
* - Quantity update: O(1)
* - Price update: O(log N)
* - Trade execution: O(1) to O(log N)
*
* All execution reports (OrderAck, OrderRej, ModifyAck, ModifyRej,
* CancelAck, CancelRej, and Trade) are generated directly by LC++ v3.
* The adapter acts purely as a translation layer and does not emit
* reports on its own.
*/

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#include "matching_engine_api.h"

#include "order_book_v3.hpp"

#if defined(__aarch64__)
static inline void cpu_pause() {
  asm volatile("yield" ::: "memory");
}
#elif defined(__x86_64__) || defined(__i386__)
  #include <immintrin.h>

static inline void cpu_pause() {
  _mm_pause();
}
#else
static inline void cpu_pause() {
}
#endif

namespace {

Price set_price(uint64_t ext_id, Price price);

const me_transport_t* g_transport = nullptr;
void* g_sink = nullptr;

void push_report(const me_report_t& r) {
  while(! g_transport->push(g_sink, &r)) {
    cpu_pause();
  }
}

void emit_ack(uint8_t type, uint64_t seq, uint64_t order_id, uint8_t side, int64_t price, uint32_t qty) {
  me_report_t r{};
  r.type = type;
  r.sequence_number = seq;
  r.order_id = order_id;
  r.side = side;
  r.price_ticks = price;
  r.quantity = qty;
  push_report(r);
}

struct EventHandler {
  void orderACK(Id seq, Id id, Side side, Price price, Qty qty) {
    emit_ack(ME_ORDER_ACK, seq, id, (-side + 1) / 2, price, qty);
  }

  void modifyACK(Id seq, Id id, Side side, Price newPrice, Qty newQty) {
    set_price(id, newPrice);
    emit_ack(ME_MODIFY_ACK, seq, id, (-side + 1) / 2, newPrice, newQty);
  }

  void modifyReject(Id seq, Id id) {
    emit_ack(ME_MODIFY_REJECT, seq, id, 0, 0, 0);
  }

  void cancelACK(Id seq, Id id, Side side, Price price, Qty qty) {
    emit_ack(ME_CANCEL_ACK, seq, id, (-side + 1) / 2, price, qty);
  }

  void cancelReject(Id seq, Id id) {
    emit_ack(ME_CANCEL_REJECT, seq, id, 0, 0, 0);
  }

  void trade(Id seq, Id maker, Id taker, Price price, Qty qty) {
    me_report_t r{};
    r.type = ME_TRADE;
    r.sequence_number = seq;
    r.order_id = maker;
    r.side = 0;
    r.price_ticks = price;
    r.maker_order_id = maker;
    r.taker_order_id = taker;
    r.quantity = qty;
    push_report(r);
  }
};

std::vector<Side> g_side;
std::vector<Index> g_slot_id;
std::vector<Price> g_price;
std::vector<v3::Order*> g_orders;

EventHandler* g_events = nullptr;
v3::OrderBook<EventHandler>* g_book = nullptr;

inline v3::Order* find_order(uint64_t ext_id) {
  return ext_id < g_orders.size() ? g_orders[ext_id] : nullptr;
}

inline uint8_t find_side(uint64_t ext_id) {
  return ext_id < g_side.size() ? g_side[ext_id] : 0;
}

inline Price find_price(uint64_t ext_id) {
  return ext_id < g_price.size() ? g_price[ext_id] : 0;
}

inline Price set_price(uint64_t ext_id, Price price) {
  if(ext_id < g_price.size()) {
    g_price[ext_id] = price;
  }

  return price;
}

} // namespace

extern "C" {

void engine_init(uint64_t /*seed*/, const me_transport_t* transport, void* report_sink) {
  g_transport = transport;
  g_sink = report_sink;

  g_events = new EventHandler();
  g_book = new v3::OrderBook<EventHandler>(*g_events);

  g_orders.resize(1u << 21);
  g_side.resize(1u << 21);
  g_price.resize(1u << 21);
  g_slot_id.resize(1u << 21);
}

void engine_shutdown(void) {
  for(v3::Order* order : g_orders) {
    delete order;
  }

  g_orders.clear();
  g_side.clear();
  g_price.clear();
  g_slot_id.clear();

  delete g_book;
  g_book = nullptr;

  delete g_events;
  g_events = nullptr;
}

void engine_flush(void) {
}

void engine_prebuild(uint8_t msg_type, const void* msg) {
}

void engine_on_new_order(const new_order_t* o) {
  g_side[o->order_id] = o->side;
  g_price[o->order_id] = o->price_ticks;

  if(o->ioc) {
    if(o->side == 0) {
      g_book->insert_ioc_order<Buy>(o->order_id, 
                                    o->price_ticks, 
                                    o->quantity,
                                    o->sequence_number);
    } else {
      g_book->insert_ioc_order<Sell>(o->order_id, 
                                     o->price_ticks, 
                                     o->quantity, 
                                     o->sequence_number);
    }
  } else {
    if(o->side == 0) {
      g_slot_id[o->order_id] = g_book->insert_order<Buy>(o->order_id, 
                                                         o->price_ticks, 
                                                         o->quantity, 
                                                         o->sequence_number);
    } else {
      g_slot_id[o->order_id] = g_book->insert_order<Sell>(o->order_id, 
        o->price_ticks, o->quantity, o->sequence_number);
    }
  }
}

void engine_on_cancel(const cancel_t* c) {
  auto& qcid = g_slot_id[c->order_id];
  uint8_t side = find_side(c->order_id);
  Price price = find_price(c->order_id);

  if(side == 0) {
    g_book->cancel_order<Buy>(c->order_id, 
                              price, 
                              qcid, 
                              c->sequence_number);
  } else {
    g_book->cancel_order<Sell>(c->order_id, 
                               price, 
                               qcid, 
                               c->sequence_number);
  }
}

void engine_on_modify(const modify_t* m) {
  auto& qcid = g_slot_id[m->order_id];
  uint8_t side = find_side(m->order_id);
  Price price = find_price(m->order_id);

  if(side == 0) {
    qcid = g_book->update_order<Buy>(m->order_id, 
                                     price, 
                                     m->new_price_ticks, 
                                     m->new_quantity, 
                                     qcid, 
                                     m->sequence_number);
  } else {
    qcid = g_book->update_order<Sell>(m->order_id, 
                                      price, 
                                      m->new_price_ticks, 
                                      m->new_quantity, 
                                      qcid, 
                                      m->sequence_number);
  }
}

int64_t engine_query_best_bid(void) {
  const Price best = g_book->best_bid();

  if(best == MinPrice - 1) {
    return INT64_MIN;
  } else {
    return best;
  }

  return best;
}

int64_t engine_query_best_ask(void) {
  const Price best = g_book->best_ask();

  if(best == MaxPrice + 1) {
    return INT64_MAX;
  } else {
    return best;
  }

  return best;
}

uint64_t engine_query_depth_at(int64_t price_ticks, uint8_t side) {
  if(side == 0) {
    return g_book->_ext_depth_at<Buy>(price_ticks);
  } else {
    return g_book->_ext_depth_at<Sell>(price_ticks);
  }
}

} // extern "C"
