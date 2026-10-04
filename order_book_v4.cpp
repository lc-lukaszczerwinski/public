/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include "order_book_v4.hpp"
#include "ring_buffer.hpp"
#include "test_utils.hpp"
#include "timer.hpp"

struct EventHandler {
  void orderACK(Id seq, Id id, Side side, Price price, Qty qty) {
    buffer.push(OrderACK(seq, id, side, price, qty));
  }

  void modifyACK(Id seq, Id id, Side side, Price newPrice, Qty newQty) {
    buffer.push(ModifyACK(seq, id, side, newPrice, newQty));
  }

  void modifyReject(Id seq, Id id) {
    buffer.push(ModifyReject(seq, id));
  }

  void cancelACK(Id seq, Id id, Side side, Price price, Qty qty) {
    buffer.push(CancelACK(seq, id, side, price, qty));
  }

  void cancelReject(Id seq, Id id) {
    buffer.push(CancelReject(seq, id));
  }

  void trade(Id seq, Id maker, Id taker, Price price, Qty qty) {
    buffer.push(Trade(seq, maker, taker, price, qty));
  }

  Event pop() {
    Event event;
    
    while(buffer.pop(event) == false) {
      _mm_pause();
    }

    return event;
  }

  std::size_t clear() noexcept {
    Event event;
    std::size_t count = 0;

    while(buffer.empty() == false) {
      count += 1;
      buffer.pop(event);
    }

    return count;
  }

  std::size_t log(const std::string& prefix = "") {
    std::size_t count = 0;

    while(buffer.empty() == false) {
      count += 1;

      Event event;
      buffer.pop(event);

      std::cout << prefix << event << std::endl;
    }

    return count;
  }

  RingBuffer<Event, 128> buffer;
};

using OrderBookV4 = v4::OrderBook<EventHandler>;

template<typename OBook>
void test_v4(int32_t price) {
  test_insert<OBook, Sell>(price);
  test_insert<OBook, Buy>(price);

  test_delete<OBook, Sell>(price);
  test_delete<OBook, Buy>(price);

  test_double_delete<OBook, Sell>(price);
  test_double_delete<OBook, Buy>(price);

  test_trade<OBook, Sell>(price);
  test_trade<OBook, Buy>(price);

  test_trade_level<OBook, Sell>(price);
  test_trade_level<OBook, Buy>(price);

  test_trend<OBook>(price, +1);
  test_trend<OBook>(price, -1);
}

int main(int argc, char* argv[]) {
  if (argc > 1 && std::string(argv[1]) == "--help") {
    std::cout << "Usage: " << argv[0] << " [--help] [--utests] [--ftests] [--benchmark]\n";
    return 0;
  }

  if (argc > 1 && std::string(argv[1]) == "--utests") {
    test_v4<OrderBookV4>(1'000);
    return 0;
  }

  if (argc > 1 && std::string(argv[1]) == "--ftests") {
    benchmark<OrderBookV4>(100'000);
    return 0;
  }

  if (argc > 1 && std::string(argv[1]) == "--benchmark") {
    benchmark<OrderBookV4>(10'000'000);
    return 0;
  }

#ifdef HFT_DEBUG
  test_v4<OrderBookV4>(1'000);
  benchmark<OrderBookV4>(100'000);
#else
  benchmark<OrderBookV4>(1'000'000);
#endif

  return 0;
}
