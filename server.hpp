/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <array>
#include <atomic>
#include <immintrin.h>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include <asio.hpp>

#include "order_book_v4.hpp"
#include "protocol.hpp"
#include "ring_buffer_spsc.hpp"

using asio::ip::udp;

template<typename TProtocol /* FIX/OUCH */>
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

    constexpr int32_t sequence_size = 32;
    constexpr int32_t buffer_size = 1024;

    uint64_t expected_sequence_number = 1;
    std::array<char, buffer_size> buffer{};
    std::array<OrderCommand, sequence_size> sequence_buffer{};

    while (running) {
      asio::error_code error;
      const std::size_t available = socket.available(error);

      if (error) {
        std::cerr << "UDP receive availability check failed: " << error.message() << '\n';
        return;
      }

      if (available == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      const std::size_t size = socket.receive_from(asio::buffer(buffer), ::ref(client_endpoint), 0, error);

      if (error) {
        std::cerr << "UDP receive failed: " << error.message() << '\n';
        return;
      }

      client_known.store(true, std::memory_order_release);
      const auto command = protocol.decode_command(WireMessage{buffer.data(), size});

      if (! command) {
        std::cerr << "Unsupported protocol message.\n";
        continue;
      }

      if constexpr (! TProtocol::has_sequence_numbers) {
        _enqueue(*command);
        continue;
      }

      if (command->sequenceNumber < expected_sequence_number) {
        std::cerr << "Order duplicated: " << command->sequenceNumber << '\n';
        continue;
      }

      if (command->sequenceNumber - expected_sequence_number >= sequence_size) {
        std::cerr << "Order too far ahead: " << command->sequenceNumber << '\n';
        continue;
      }

      sequence_buffer[command->sequenceNumber & (sequence_size - 1)] = *command;

      while (sequence_buffer[expected_sequence_number & (sequence_size - 1)].sequenceNumber == expected_sequence_number) {
        const auto next = sequence_buffer[expected_sequence_number & (sequence_size - 1)];
        expected_sequence_number += 1;
        _enqueue(next);
      }
    }
  }

  void _enqueue(const OrderCommand& command) {
    while (! queue_in.push(command)) {
      _mm_pause();
    }
  }

  void _ex_thread() {
    while (running) {
      OrderCommand command;

      if (! queue_in.pop(command)) {
        _mm_pause();
        continue;
      }

      switch (command.type) {
      case OrderCommandType::LIMIT:
        _execute_limit_order(command);
        break;
      case OrderCommandType::IOC:
        _execute_ioc_order(command);
        break;
      case OrderCommandType::MARKET:
        _execute_market_order(command);
        break;
      case OrderCommandType::CANCEL:
        _execute_cancel_order(command);
        break;
      }
    }
  }

  void _execute_limit_order(const OrderCommand& command) {
    if (command.side == Buy) {
      order_book.template insert_order<Buy>(command.clOrdId, command.price, command.qty);
    } else {
      order_book.template insert_order<Sell>(command.clOrdId, command.price, command.qty);
    }
  }

  void _execute_ioc_order(const OrderCommand& command) {
    if (command.side == Buy) {
      order_book.template insert_ioc_order<Buy>(command.clOrdId, command.price, command.qty);
    } else {
      order_book.template insert_ioc_order<Sell>(command.clOrdId, command.price, command.qty);
    }
  }

  void _execute_market_order(const OrderCommand& command) {
    if (command.side == Buy) {
      order_book.template insert_mkt_order<Buy>(command.clOrdId, command.qty);
    } else {
      order_book.template insert_mkt_order<Sell>(command.clOrdId, command.qty);
    }
  }

  void _execute_cancel_order(const OrderCommand& command) {
    assert(command.orderId == command.origClOrdId);

    if (command.side == Buy) {
      order_book.template cancel_order<Buy>(command.origClOrdId, command.orderId, command.price, command.clOrdId);
    } else {
      order_book.template cancel_order<Sell>(command.origClOrdId, command.orderId, command.price, command.clOrdId);
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

      for (const auto& message : protocol.encode_event(event)) {
        socket.send_to(asio::buffer(message), client_endpoint);
      }
    }
  }

  struct EventHandler {
    RingBufferSPSC<OrderCommand, 64>& queue_in;
    RingBufferSPSC<Event, 64>& queue_out;

    void _push(Event event) {
      while (! queue_out.push(event)) {
        _mm_pause();
      }
    }

    void onOrderAccepted(Id id, Index slot, Side side, Price orderPrice, Qty leavesQty) {
      _push(Event{.type = ORDER_ACCEPTED, //
                  .makerSide = side,
                  .makerId = id,
                  .makerSlot = slot,
                  .makerPrice = orderPrice,
                  .makerLeavesQty = leavesQty});
    }

    void onOrderCanceled(Id id, Index slot, Side side, Price orderPrice, Qty cumQty, Id requestId = -1) {
      _push(Event{.type = ORDER_CANCELED, //
                  .makerSide = side,
                  .takerId = requestId,
                  .makerId = id,
                  .makerSlot = slot,
                  .makerPrice = orderPrice,
                  .makerCumQty = cumQty});
    }

    void onCancelRejected(Id id, Index, Side side, Price, Id requestId = -1) {
      _push(Event{.type = CANCEL_REJECTED, //
                  .makerSide = side,
                  .takerId = requestId,
                  .makerId = id});
    }

    void onTrade(Id takerId,
                 Index takerSlot,
                 Side takerSide,
                 Price takerPrice,
                 Qty takerCumQty,
                 Qty takerLeavesQty,
                 Id makerId,
                 Index makerSlot,
                 Side makerSide,
                 Price makerPrice,
                 Qty makerCumQty,
                 Qty makerLeavesQty,
                 Price lastPrice,
                 Qty lastQty) //
    {
      _push(Event{.type = ORDER_TRADED,
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

  RingBufferSPSC<OrderCommand, 64> queue_in;
  RingBufferSPSC<Event, 64> queue_out;

  std::atomic<bool> running{true};
  std::atomic<bool> client_known{false};

  udp::endpoint client_endpoint;

  EventHandler event_handler{queue_in, queue_out};
  v4::OrderBook<EventHandler> order_book{event_handler};
  TProtocol protocol;
};
