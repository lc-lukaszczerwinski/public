/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#include <asio.hpp>

#include "protocol.hpp"

using asio::ip::udp;

constexpr const char* BLUE_PASTEL = "\033[38;5;117m";
constexpr const char* RED_PASTEL = "\033[38;5;210m";
constexpr const char* GREY = "\033[38;5;245m";

inline std::string bold(const std::string& text) {
  return "\033[1m" + text + "\033[22m";
}

inline std::string italic(const std::string& text) {
  return "\033[3m" + text + "\033[23m";
}

inline std::string blue_pastel(const std::string& text) {
  return BLUE_PASTEL + text + "\033[39m";
}

inline std::string grey(const std::string& text) {
  return GREY + text + "\033[39m";
}

inline std::string red_pastel(const std::string& text) {
  return RED_PASTEL + text + "\033[39m";
}

template<typename TProtocol>
class Client {
public:

  explicit Client() {
    socket.open(udp::v4());
    socket.bind(udp::endpoint{asio::ip::address_v4::loopback(), 0});
  }

  void run() {
    std::string line;
    std::thread rx{&Client::_rx_thread, this};

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::cout << "> " << std::flush;

      if (! std::getline(std::cin, line)) {
        break;
      }

      std::istringstream input{line};
      std::string type;
      input >> type;

      if (type == "#") {
        input >> m_msgSeqNum >> type;
      }

      if (type == "exit") {
        break;
      }

      if (type == "help" || type == "?" || type == "h") {
        _print_help();
        continue;
      }

      std::optional<OrderCommand> command = _read_command(type, input);

      if (! command) {
        std::cerr << "Unsupported command: " << type << '\n';
        continue;
      }

      if constexpr (TProtocol::has_sequence_numbers) {
        command->sequenceNumber = m_msgSeqNum++;
      }
      const WireMessage message = protocol.encode_command(*command);
      socket.send_to(asio::buffer(message), server_endpoint);
    }

    m_running.store(false, std::memory_order_release);
    rx.join();
  }

private:

  std::optional<OrderCommand> _read_command(const std::string& type, std::istringstream& input) {
    if (type == "lmt") {
      return _new_order(OrderCommandType::LIMIT, input);
    }

    if (type == "ioc") {
      return _new_order(OrderCommandType::IOC, input);
    }

    if (type == "mkt") {
      return _new_order(OrderCommandType::MARKET, input);
    }

    if (type == "cnl") {
      return _cancel_order(input);
    }

    return std::nullopt;
  }

  OrderCommand _new_order(OrderCommandType type, std::istringstream& input) {
    char separator{};
    char side{};
    input >> side;

    OrderCommand command{.type = type, .side = side == 'b' ? Buy : Sell, .clOrdId = m_clOrdId++};

    input >> command.qty;

    if (type != OrderCommandType::MARKET) {
      input >> separator >> command.price;
    }

    return command;
  }

  OrderCommand _cancel_order(std::istringstream& input) {
    char side{};
    Id originalOrderId{};
    input >> side >> originalOrderId;

    return OrderCommand{
        .type = OrderCommandType::CANCEL, .side = side == 'b' ? Buy : Sell, .clOrdId = m_clOrdId++, .orderId = originalOrderId, .origClOrdId = originalOrderId};
  }

  void _print_help() {
    std::string sequencePrefix;
    if constexpr (TProtocol::has_sequence_numbers) {
      sequencePrefix = grey(italic("[# <msgSeqNum>]")) + " ";
    }

    std::cout << "  " << sequencePrefix << blue_pastel(bold("lmt")) << " <b|s> <quantity> @ <price>  Limit order (GTC)\n"
              << "  " << sequencePrefix << blue_pastel(bold("ioc")) << " <b|s> <quantity> @ <price>  Limit order (IOC)\n"
              << "  " << sequencePrefix << blue_pastel(bold("mkt")) << " <b|s> <quantity>            Market order (IOC)\n"
              << "  " << sequencePrefix << blue_pastel(bold("cnl")) << " <b|s> <orderId>             Cancel order using the " << italic("clientOrderId") << "\n"
              << "  " << grey(italic("               ")) << " " << blue_pastel(bold("exit")) << "                            Quit client\n"
              << std::flush;
  }

  void _rx_thread() {
    udp::endpoint remote;
    std::array<char, 1024> buffer{};

    while (m_running.load(std::memory_order_acquire)) {
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

      const std::size_t size = socket.receive_from(asio::buffer(buffer), remote, 0, ::ref(error));

      if (error) {
        std::cerr << "UDP receive failed: " << error.message() << '\n';
        return;
      }

      if (size == 0) {
        continue;
      }

      const auto event = protocol.decode_event(WireMessage{buffer.data(), size});

      if (! event) {
        std::cerr << "Unsupported protocol message.\n";
        continue;
      }

      _print_event(*event);
    }
  }

  void _print_event(const OrderEvent& event) {
    const auto side = event.side == Buy ? "buy" : "sell";

    if (event.type == OrderEventType::ACCEPTED) {
      const std::string message = std::format("Order added: side={} clOrdId={} orderId={} leavesQty={} orderPrice={}\n", //
                                              side,
                                              event.clOrdId,
                                              event.orderId,
                                              event.leavesQty,
                                              event.orderPrice);

      std::cout << "    " << blue_pastel(message) << std::flush;
    } else if (event.type == OrderEventType::TRADED) {
      const std::string message = std::format("Order traded: side={} clOrdId={} orderId={} orderPrice={} lastPrice={} cumQty={} leavesQty={} lastQty={}\n", //
                                              side,
                                              event.clOrdId,
                                              event.orderId,
                                              event.orderPrice,
                                              event.lastPrice,
                                              event.cumQty,
                                              event.leavesQty,
                                              event.lastQty);

      std::cout << "    " << blue_pastel(message) << std::flush;
    } else if (event.type == OrderEventType::CANCELED) {
      const std::string message = std::format("Order canceled: clOrdId={} side={} orderPrice={} cumQty={} leavesQty={}\n", //
                                              event.clOrdId,
                                              side,
                                              event.orderPrice,
                                              event.cumQty,
                                              event.leavesQty);

      std::cout << "    " << blue_pastel(message) << std::flush;
    } else if (event.type == OrderEventType::CANCEL_REJECTED) {
      const std::string message = std::format("Cancel rejected: clOrdId={} orderId={} origClOrdId={} ordStatus={} side={}\n", //
                                              event.clOrdId,
                                              event.orderId,
                                              event.origClOrdId,
                                              event.status,
                                              side);

      std::cout << "    " << red_pastel(message) << std::flush;
    }
  }

public:

  Id m_clOrdId{1};
  int64_t m_msgSeqNum{1};
  std::atomic<bool> m_running{true};
  asio::io_context io;
  udp::socket socket{io};
  udp::endpoint server_endpoint{asio::ip::make_address("127.0.0.1"), 9000};
  TProtocol protocol;
};
