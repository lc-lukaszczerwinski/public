/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <immintrin.h>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include <asio.hpp>

#include "FIX.hpp"
#include "order_book_v4.hpp"
#include "ring_buffer_spsc.hpp"

using asio::ip::udp;

struct Order {
  Side side;
  TimeInForce tif;
  int64_t clOrdId;
  int64_t orderId;
  int64_t origClOrdId;
  int64_t size;
  int64_t price;
  int64_t msgSeqNum;
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
    std::thread rx(&Server::_rx_thread, this);
    std::thread ex(&Server::_ex_thread, this);
    std::thread tx(&Server::_tx_thread, this);

    std::cout << "Server running...\n";

    std::string line;
    std::getline(std::cin, line);

    running = false;

    rx.join();
    ex.join();
    tx.join();
  }

private:

  void _rx_thread() {
    asio::io_context io;
    udp::socket socket(io, udp::endpoint(udp::v4(), 9000));

    uint64_t expSeqNum = 1;
    constexpr int32_t seqSize = 32;
    constexpr int32_t bufferSize = 1024;

    std::array<char, bufferSize> buffer{};
    std::array<Order, seqSize> seqBuffer{};

    while (running) {
      const auto size = socket.receive_from(asio::buffer(buffer), client_endpoint);
      client_known.store(true, std::memory_order_release);
      std::string message{buffer.data(), size};

      Order order{};
      const auto msgSeqNum = FIX::stol(FIX::tag(message, FIX::MsgSeqNum_34));
      const auto msgType = FIX::stoc(FIX::tag(message, FIX::MsgType_35));

      if (msgType == FIX::MsgType_NewOrderSingle) {
        order = _parse_new_order(std::move(message));
      } else if (msgType == FIX::MsgType_OrderCancelRequest) {
        order = _parse_cancel_order(std::move(message));
      } else {
        std::cerr << "Unsupported FIX MsgType: " << msgType << '\n';
        continue;
      }

      order.msgSeqNum = msgSeqNum;

      if (order.msgSeqNum < expSeqNum) {
        std::cerr << "Order duplicated: " << order.msgSeqNum << '\n';
        continue;
      }

      if (order.msgSeqNum - expSeqNum >= seqSize) {
        std::cerr << "Order too far ahead: " << order.msgSeqNum << '\n';
        continue;
      }

      seqBuffer[order.msgSeqNum & (seqSize - 1)] = order;

      while (seqBuffer[expSeqNum & (seqSize - 1)].msgSeqNum == expSeqNum) {
        order = seqBuffer[expSeqNum & (seqSize - 1)];
        assert(order.msgSeqNum == expSeqNum);
        expSeqNum += 1;

        while (! queue_in.push(std::move(order))) {
          _mm_pause();
        }
      }
    }
  }

  Order _parse_new_order(std::string message) {
    Order order{};
    order.clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
    order.size = FIX::stol(FIX::tag(message, FIX::Quantity_38));
    const auto ordType = FIX::stoc(FIX::tag(message, FIX::OrdType_40));

    if (ordType == FIX::OrdType_Limit) {
      order.price = FIX::stol(FIX::tag(message, FIX::OrderPrice_44));
    } else if (ordType == FIX::OrdType_Market) {
      order.price = 0;
    }

    order.side = FIX::stoc(FIX::tag(message, FIX::Side_54)) == FIX::Buy ? Buy : Sell;
    order.tif = FIX::stoc(FIX::tag(message, FIX::TimeInForce_59)) == FIX::TimeInForce_IOC ? IOC : GTC;
    return order;
  }

  Order _parse_cancel_order(std::string message) {
    Order order{};
    order.clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
    order.orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
    order.origClOrdId = FIX::stol(FIX::tag(message, FIX::OrigClOrdId_41));
    order.side = FIX::stoc(FIX::tag(message, FIX::Side_54)) == FIX::Buy ? Buy : Sell;
    order.size = 0;
    return order;
  }

  void _ex_thread() {
    while (running) {
      Order order;

      if (! queue_in.pop(order)) {
        _mm_pause();
        continue;
      }

      if (order.tif == GTC && order.size != 0 && order.price != 0) {
        _execute_limit_order(order);
      } else if (order.tif == IOC && order.size != 0 && order.price != 0) {
        _execute_ioc_order(order);
      } else if (order.tif == IOC && order.size != 0 && order.price == 0) {
        _execute_market_order(order);
      } else if (order.size == 0) {
        _execute_cancel_order(order);
      }
    }
  }

  void _execute_limit_order(const Order& order) {
    if (order.side == Buy) {
      order_book.insert_order<Buy>(order.clOrdId, order.price, order.size);
    } else {
      order_book.insert_order<Sell>(order.clOrdId, order.price, order.size);
    }
  }

  void _execute_ioc_order(const Order& order) {
    if (order.side == Buy) {
      order_book.insert_ioc_order<Buy>(order.clOrdId, order.price, order.size);
    } else {
      order_book.insert_ioc_order<Sell>(order.clOrdId, order.price, order.size);
    }
  }

  void _execute_market_order(const Order& order) {
    if (order.side == Buy) {
      order_book.insert_mkt_order<Buy>(order.clOrdId, order.size);
    } else {
      order_book.insert_mkt_order<Sell>(order.clOrdId, order.size);
    }
  }

  void _execute_cancel_order(const Order& order) {
    assert(order.orderId == order.origClOrdId);

    if (order.side == Buy) {
      order_book.cancel_order<Buy>(order.origClOrdId, order.orderId, order.price, order.clOrdId);
    } else {
      order_book.cancel_order<Sell>(order.origClOrdId, order.orderId, order.price, order.clOrdId);
    }
  }

  void _tx_thread() {
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

      if (event.type == Type::ORDER_ACCEPTED) {
        _send_order_accepted(event, socket);
      } else if (event.type == Type::ORDER_TRADED) {
        _send_trade_reports(event, socket);
      } else if (event.type == Type::ORDER_CANCELED) {
        _send_order_canceled(event, socket);
      } else if (event.type == Type::CANCEL_REJECTED) {
        _send_cancel_rejected(event, socket);
      }
    }
  }

  void _send_order_accepted(const Event& event, udp::socket& socket) {
    std::string fix;

    fix += FIX::tag(FIX::MsgSeqNum_34, m_msgSeqNum++);
    fix += FIX::tag(FIX::MsgType_35, FIX::MsgType_ExecutionReport);
    fix += FIX::tag(FIX::ExecType_150, FIX::ExecType_New);

    fix += FIX::tag(FIX::ClOrdId_11, event.makerId);
    fix += FIX::tag(FIX::OrderId_37, event.makerSlot);
    fix += FIX::tag(FIX::OrdStatus_39, FIX::OrdStatus_New);
    fix += FIX::tag(FIX::OrderPrice_44, event.makerPrice);
    fix += FIX::tag(FIX::Side_54, event.makerSide == Buy ? FIX::Buy : FIX::Sell);
    fix += FIX::tag(FIX::LeavesQty_151, event.makerLeavesQty);
    socket.send_to(asio::buffer(fix), client_endpoint);
  }

  void _send_trade_reports(const Event& event, udp::socket& socket) {
    const auto send_trade_report = [&](Id id, Side side, Price orderPrice, Qty cumQty, Qty leavesQty) {
      const char ordStatus = leavesQty == 0 //
                                 ? FIX::OrdStatus_Filled
                                 : FIX::OrdStatus_PartiallyFilled;

      std::string fix;

      fix += FIX::tag(FIX::MsgSeqNum_34, m_msgSeqNum++);
      fix += FIX::tag(FIX::MsgType_35, FIX::MsgType_ExecutionReport);
      fix += FIX::tag(FIX::ExecType_150, FIX::ExecType_Trade);

      fix += FIX::tag(FIX::OrdStatus_39, ordStatus);
      fix += FIX::tag(FIX::Side_54, side == Buy ? FIX::Buy : FIX::Sell);
      fix += FIX::tag(FIX::ClOrdId_11, id);
      fix += FIX::tag(FIX::OrderId_37, id);
      fix += FIX::tag(FIX::OrderPrice_44, orderPrice);
      fix += FIX::tag(FIX::CumQty_14, cumQty);
      fix += FIX::tag(FIX::LeavesQty_151, leavesQty);
      fix += FIX::tag(FIX::LastQty_32, event.lastQty);
      fix += FIX::tag(FIX::LastPrice_31, event.lastPrice);
      socket.send_to(asio::buffer(fix), client_endpoint);
    };

    send_trade_report(event.takerId, event.takerSide, event.takerPrice, event.takerCumQty, event.takerLeavesQty);
    send_trade_report(event.makerId, event.makerSide, event.makerPrice, event.makerCumQty, event.makerLeavesQty);
  }

  void _send_order_canceled(const Event& event, udp::socket& socket) {
    std::string fix;

    fix += FIX::tag(FIX::MsgSeqNum_34, m_msgSeqNum++);
    fix += FIX::tag(FIX::MsgType_35, FIX::MsgType_ExecutionReport);
    fix += FIX::tag(FIX::ExecType_150, FIX::ExecType_Canceled);

    fix += FIX::tag(FIX::ClOrdId_11, /* reuse takerId for ClOrdId */ event.takerId);
    fix += FIX::tag(FIX::CumQty_14, event.makerCumQty);
    fix += FIX::tag(FIX::OrderId_37, event.makerSlot);
    fix += FIX::tag(FIX::OrdStatus_39, FIX::OrdStatus_Canceled);
    fix += FIX::tag(FIX::OrigClOrdId_41, event.makerId);
    fix += FIX::tag(FIX::OrderPrice_44, event.makerPrice);
    fix += FIX::tag(FIX::Side_54, event.makerSide == Buy ? FIX::Buy : FIX::Sell);
    fix += FIX::tag(FIX::LeavesQty_151, 0);
    socket.send_to(asio::buffer(fix), client_endpoint);
  }

  void _send_cancel_rejected(const Event& event, udp::socket& socket) {
    std::string fix;

    fix += FIX::tag(FIX::MsgSeqNum_34, m_msgSeqNum++);
    fix += FIX::tag(FIX::MsgType_35, FIX::MsgType_CancelRejected);

    fix += FIX::tag(FIX::ClOrdId_11, event.takerId);
    fix += FIX::tag(FIX::OrderId_37, event.makerId);
    fix += FIX::tag(FIX::OrdStatus_39, FIX::OrdStatus_Rejected);
    fix += FIX::tag(FIX::OrigClOrdId_41, event.makerId);
    fix += FIX::tag(FIX::Side_54, event.makerSide == Buy ? FIX::Buy : FIX::Sell);
    socket.send_to(asio::buffer(fix), client_endpoint);
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

    void onOrderAccepted(Id id, Index slot, Side side, Price orderPrice, Qty leavesQty) {
      _push(
          Event{
              .type = ORDER_ACCEPTED, //
              .makerSide = side,
              .makerId = id,
              .makerSlot = slot,
              .makerPrice = orderPrice,
              .makerLeavesQty = leavesQty});
    }

    void onOrderCanceled(Id id, Index slot, Side side, Price orderPrice, Qty cumQty, Id req = -1) {
      _push(
          Event{
              .type = ORDER_CANCELED, //
              .makerSide = side,
              .takerId = /* reuse takerId for req */ req,
              .makerId = id,
              .makerSlot = slot,
              .makerPrice = orderPrice,
              .makerCumQty = cumQty});
    }

    void onCancelRejected(Id id, Index slot, Side side, Price /* orderPrice */, Id req = -1) {
      _push(
          Event{
              .type = CANCEL_REJECTED, //
              .makerSide = side,
              .takerId = req,
              .makerId = id});
    }

    void onTrade( //
      Id takerId, Index takerSlot, Side takerSide, Price takerPrice, Qty takerCumQty, Qty takerLeavesQty,
      Id makerId, Index makerSlot, Side makerSide, Price makerPrice, Qty makerCumQty, Qty makerLeavesQty,
      Price lastPrice, Qty lastQty
    ) {
      _push(
          Event{
              .type = ORDER_TRADED, //
              .takerSide = takerSide,
              .makerSide = makerSide,
              .takerId = takerId,
              .makerId = makerId,
              .takerSlot = takerSlot,
              .makerSlot = makerSlot,
              .takerPrice = takerPrice,
              .makerPrice = makerPrice,
              .takerCumQty = takerCumQty,
              .makerCumQty = makerCumQty,
              .takerLeavesQty = takerLeavesQty,
              .makerLeavesQty = makerLeavesQty,
              .lastPrice = lastPrice,
              .lastQty = lastQty});
    }
  };

private:

  RingBufferSPSC<Order, 64> queue_in;
  RingBufferSPSC<Event, 64> queue_out;

  int64_t m_msgSeqNum{1};
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
