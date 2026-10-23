/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include <asio.hpp>

#include <array>
#include <atomic>
#include <format>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include "FIX.hpp"

using asio::ip::udp;

constexpr const char* BLUE_LIGHT = "\033[94m";
constexpr const char* GREEN_LIGHT = "\033[92m";
constexpr const char* CYAN = "\033[0;96m";
constexpr const char* RESET = "\033[0m";

struct Client {

  explicit Client(int clOrdId)
    : m_clOrdId(clOrdId) //
  {
    socket.open(udp::v4());
    socket.bind(udp::endpoint{asio::ip::address_v4::loopback(), 0});
  }

  void run() {
    std::string line;
    std::string type;
    std::thread rx{&Client::rx_thread, this};

    while (std::getline(std::cin, line)) {
      std::istringstream iss{line};
      iss >> type;

      if (type == "lmt") {
        char at;
        char side;
        int64_t quantity;
        int64_t orderPrice;

        iss >> side >> quantity >> at >> orderPrice;

        std::string fix;
        fix += FIX::tag(FIX::MsgType /* 35 */, FIX::NewOrderSingle);
        fix += FIX::tag(FIX::OrdType /* 40 */, FIX::Limit);
        fix += FIX::tag(FIX::Side /* 54 */, (side == 'b' ? FIX::Buy : FIX::Sell));
        fix += FIX::tag(FIX::ClOrdId /* 11 */, m_clOrdId++);
        fix += FIX::tag(FIX::Quantity /* 38 */, quantity);
        fix += FIX::tag(FIX::OrderPrice /* 44 */, orderPrice);

        socket.send_to(asio::buffer(fix), server_endpoint);
      }
    }

    rx.join();
  }

private:

  void rx_thread() {
    std::array<char, 1024> buffer{};
    udp::endpoint remote;

    while (true) {
      std::size_t size = socket.receive_from(asio::buffer(buffer), remote);

      if (size == 0) {
        continue;
      }

      std::string message{buffer.data(), size};
      process_message(std::move(message));
    }
  }

  void process_message(std::string message) {
    char msg_type = FIX::stoc(FIX::tag(message, FIX::MsgType));
    char exec_type = FIX::stoc(FIX::tag(message, FIX::ExecType));
    char ord_status = FIX::stoc(FIX::tag(message, FIX::OrdStatus));

    char side;
    int64_t slot;
    int64_t cumQty;
    int64_t lastQty;
    int64_t clOrdId;
    int64_t leavesQty;
    int64_t lastPrice;
    int64_t orderPrice;

    if (exec_type == FIX::ExecType_New && ord_status == FIX::OrdStatus_New) {
      clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId));
      side = FIX::stoc(FIX::tag(message, FIX::Side));
      orderPrice = FIX::stol(FIX::tag(message, FIX::OrderPrice));
      leavesQty = FIX::stol(FIX::tag(message, FIX::LeavesQty));

      message = std::format(
          "Order added: clOrdId={} side={} quantity={} price={}\n", //
          clOrdId,
          (side == '1' ? "buy" : "sell"),
          leavesQty,
          orderPrice);

      std::cout << CYAN << "    " << message << RESET << std::flush;
    }

    if (exec_type == FIX::ExecType_Trade) {
      clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId));
      side = FIX::stoc(FIX::tag(message, FIX::Side));
      lastPrice = FIX::stol(FIX::tag(message, FIX::LastPrice));
      lastQty = FIX::stol(FIX::tag(message, FIX::LastQty));
      leavesQty = FIX::stol(FIX::tag(message, FIX::LeavesQty));

      message = std::format(
          "Order traded: clOrdId={} side={} lastPrice={} lastQty={} leavesQty={}\n", //
          clOrdId,
          (side == FIX::Buy ? "buy" : "sell"),
          lastPrice,
          lastQty,
          leavesQty);

      std::cout << CYAN << "    " << message << RESET << std::flush;
    }
  }

public:

  int m_clOrdId;
  asio::io_context io;
  udp::socket socket{io};
  udp::endpoint server_endpoint{asio::ip::make_address("127.0.0.1"), 9000};
};

int main(int argc, char* argv[]) {
  int clOrdId = 1;

  for (int i = 1; i < argc; ++i) {
    if (std::string_view{argv[i]} != "--id") {
      std::cerr << "Unknown argument: " << argv[i] << '\n';
      return 1;
    }

    if (i + 1 >= argc) {
      std::cerr << "Missing value for --id\n";
      return 1;
    }

    try {
      clOrdId = std::stoi(argv[++i]);
    } catch (const std::invalid_argument&) {
      std::cerr << "Invalid value for --id: " << argv[i] << '\n';
      return 1;
    } catch (const std::out_of_range&) {
      std::cerr << "Value for --id is out of range: " << argv[i] << '\n';
      return 1;
    }
  }

  Client client{clOrdId};
  client.run();
}
