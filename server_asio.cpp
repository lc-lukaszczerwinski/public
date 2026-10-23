/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include <asio.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <iostream>
#include <string>
#include <thread>

#include <immintrin.h>

#include "FIX.hpp"
#include "order_book_v4.hpp"
#include "ring_buffer_spsc.hpp"

using asio::ip::udp;

struct Order {
  int64_t clOrdId;
  int64_t size;
  int64_t price;
};

struct ExecutionReport {
  Id msgSeqNum;  // 34
  Index msgType; // 35

  Id execType;  // 150
  Id ordStatus; // 39

  Side side;  // 54
  Id orderId; // 37
  Id clOdrId; // 11

  Price price;     // 44
  Price lastPrice; // 31

  Qty lastQty;   // 32
  Qty cumQty;    // 14
  Qty leavesQty; // 151
};

class Server {
public:

  void run() {
    std::thread rx(&Server::rx_thread, this);
    std::thread ex(&Server::ex_thread, this);
    std::thread tx(&Server::tx_thread, this);

    std::cout << "Server running...\n";

    std::string line;
    std::getline(std::cin, line);

    running = false;

    rx.join();
    ex.join();
    tx.join();
  }

private:

  void rx_thread() {
    asio::io_context io;
    udp::socket socket(io, udp::endpoint(udp::v4(), 9000));

    std::array<char, 1024> buffer{};

    while (running) {
      udp::endpoint remote;

      std::size_t size = socket.receive_from(asio::buffer(buffer), remote);

      client_endpoint = remote;
      client_known.store(true, std::memory_order_release);

      Order order{};
      std::string message{buffer.data(), size};

      char msg_type = FIX::stoc(FIX::tag(message, FIX::MsgType));

      if (msg_type == FIX::NewOrderSingle) {
        auto type = FIX::stoi(FIX::tag(message, FIX::OrdType));
        auto side = FIX::stoc(FIX::tag(message, FIX::Side));

        order.clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId));
        order.size = FIX::stol(FIX::tag(message, FIX::Quantity)) * (side == FIX::Buy ? 1 : -1);
        order.price = FIX::stol(FIX::tag(message, FIX::OrderPrice));
      }

      while (! queue_in.push(std::move(order))) {
        _mm_pause();
      }
    }
  }

  void ex_thread() {
    while (running) {
      Order order;

      if (! queue_in.pop(order)) {
        _mm_pause();
        continue;
      }

      if (order.size > 0 && order.price > 0) {
        order_book.insert_order<Buy>(order.clOrdId, order.price, order.size);
      } else if (order.size < 0 && order.price > 0) {
        order_book.insert_order<Sell>(order.clOrdId, order.price, -order.size);
      }

      {
        /*
         * queue_out is processed by EventHandler callbacks
         */
      }
    }
  }

  void tx_thread() {
    asio::io_context io;
    udp::socket socket(io);

    socket.open(udp::v4());

    while (running) {
      if (! client_known.load(std::memory_order_acquire)) {
        _mm_pause();
        continue;
      }

      Event event;

      if (! queue_out.pop(event)) {
        _mm_pause();
        continue;
      }

      std::string fix;

      if (event.type == Type::ORDER_ACCEPTED) {
        fix = FIX::tag(FIX::MsgType /* 35 */, FIX::ExecutionReport);
        fix += FIX::tag(FIX::ExecType /* 150 */, FIX::ExecType_New);
        fix += FIX::tag(FIX::OrdStatus /* 39 */, FIX::OrdStatus_New);

        fix += FIX::tag(FIX::ClOrdId /* 11 */, event.maker);
        fix += FIX::tag(FIX::Side /* 54 */, event.side);
        fix += FIX::tag(FIX::OrderPrice /* 44 */, event.price);
        fix += FIX::tag(FIX::LeavesQty /* 151 */, event.qty);

        socket.send_to(asio::buffer(fix), client_endpoint);
      }

      if (event.type == Type::ORDER_TRADED) {
        char ordStatus = event.leavesQty == 0 //
                             ? FIX::OrdStatus_Filled
                             : FIX::OrdStatus_PartiallyFilled;

        fix = FIX::tag(FIX::MsgType /* 35 */, FIX::ExecutionReport);
        fix += FIX::tag(FIX::ExecType /* 150 */, FIX::ExecType_Trade);
        fix += FIX::tag(FIX::OrdStatus /* 39 */, ordStatus);

        fix += FIX::tag(FIX::ClOrdId /* 11 */, event.maker);
        fix += FIX::tag(FIX::Side /* 54 */, event.side == Buy ? FIX::Buy : FIX::Sell);
        fix += FIX::tag(FIX::LastPrice /* 31 */, event.price);
        fix += FIX::tag(FIX::LastQty /* 32 */, event.qty);
        fix += FIX::tag(FIX::LeavesQty /* 151 */, event.leavesQty);
        socket.send_to(asio::buffer(fix), client_endpoint);
      }
    }
  }

private:

  struct EventHandler {
    RingBufferSPSC<Order, 64>& queue_in;
    RingBufferSPSC<Event, 64>& queue_out;

    void _push(Event event) {
      while (! queue_out.push(event)) {
        _mm_pause();
      }
    }

    /* deprecated */
    void orderACK(Id seq, Id clOrdId, Side side, Price price, Qty qty) {
      return onOrderAccepted(clOrdId, side, price, qty);
    }

    void onOrderAccepted(Id clOrdId, Side side, Price orderPrice, Qty qty) {
      _push(
          Event{
              .type = ORDER_ACCEPTED, //

              .side = side,
              .seq = -1,
              .maker = clOrdId,
              .taker = -1,
              .price = orderPrice,
              .qty = qty});
    }

    void onOrderCancelled(Id clOrdId, Index slot, Side side, Price orderPrice, Qty qty) {
    }

    void onCancelRejected(Id clOrdId, Index slot) {
    }

    /* deprecated */
    void trade(Id seq, Id maker, Id taker, Price price, Qty qty) {
      // no tests for server
    }

    void onOrderTraded(Id clOrdId, Index slot, Side side, Price orderPrice, Price lastPrice, Qty lastQty, Qty cumQty, Qty leavesQty) {
      _push(
          Event{
              .type = ORDER_TRADED, //
              .side = side,
              .seq = -1,
              .maker = clOrdId,
              .taker = -1,
              .price = lastPrice,
              .qty = lastQty,
              .leavesQty = leavesQty});
    }
  };

private:

  RingBufferSPSC<Order, 64> queue_in;
  RingBufferSPSC<Event, 64> queue_out;

  std::atomic<bool> running{true};
  std::atomic<bool> client_known{false};

  udp::endpoint client_endpoint;

  EventHandler event_handler{queue_in, queue_out};
  v4::OrderBook<EventHandler> order_book{event_handler};
};

int main() {
  Server server;
  server.run();
}
