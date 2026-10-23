/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include <array>
#include <atomic>
#include <format>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#include <asio.hpp>

#include "FIX.hpp"
#include "common.hpp"

using asio::ip::udp;

constexpr const char* GREEN_PASTEL = "\033[38;5;121m";
constexpr const char* BLUE_PASTEL = "\033[38;5;117m";
constexpr const char* CYAN_PASTEL = "\033[38;5;123m";
constexpr const char* RED_PASTEL = "\033[38;5;210m";
constexpr const char* YELLOW_PASTEL = "\033[38;5;222m";
constexpr const char* GREY = "\033[38;5;245m";
constexpr const char* LIGHT_GREY = "\033[38;5;250m";
constexpr const char* RESET = "\033[0m";

std::string bold(const std::string& text) {
  return "\033[1m" + text + "\033[22m";
}

std::string italic(const std::string& text) {
  return "\033[3m" + text + "\033[23m";
}

std::string blue_pastel(const std::string& text) {
  return BLUE_PASTEL + text + "\033[39m";
}

std::string grey(const std::string& text) {
  return GREY + text + "\033[39m";
}

std::string red_pastel(const std::string& text) {
  return RED_PASTEL + text + "\033[39m";
}

struct Client {

  explicit Client() {
    socket.open(udp::v4());
    socket.bind(udp::endpoint{asio::ip::address_v4::loopback(), 0});
  }

  void run() {
    std::string line;
    std::thread rx{&Client::rx_thread, this};

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      std::cout << "> " << std::flush;

      if (! std::getline(std::cin, line)) {
        break;
      }

      std::istringstream iss{line};
      std::string type;
      std::string fix;
      iss >> type;

      if (type == "#") {
        iss >> m_msgSeqNum >> type;
      }

      fix += FIX::tag(FIX::MsgSeqNum_34, m_msgSeqNum);

      if (type == "lmt") {
        fix += FIX::tag(FIX::MsgType_35, FIX::MsgType_NewOrderSingle);
        _limit_order(std::ref(fix), std::move(iss));
      } else if (type == "ioc") {
        fix += FIX::tag(FIX::MsgType_35, FIX::MsgType_NewOrderSingle);
        _ioc_order(std::ref(fix), std::move(iss));
      } else if (type == "mkt") {
        fix += FIX::tag(FIX::MsgType_35, FIX::MsgType_NewOrderSingle);
        _mkt_order(std::ref(fix), std::move(iss));
      } else if (type == "cnl") {
        fix += FIX::tag(FIX::MsgType_35, FIX::MsgType_OrderCancelRequest);
        _cancel_order(std::ref(fix), std::move(iss));
      } else if (type == "help" || type == "?") {
        _print_help();
        continue;
      }

      m_msgSeqNum += 1;
      socket.send_to(asio::buffer(fix), server_endpoint);
    }

    rx.join();
  }

private:

  void _print_help() {
    std::cout << "  " << grey(italic("[# <msgSeqNum>]")) << " " << blue_pastel(bold("lmt")) << " <b|s> <quantity> @ <price>  Limit order (GTC)\n"
              << "  " << grey(italic("[# <msgSeqNum>]")) << " " << blue_pastel(bold("ioc")) << " <b|s> <quantity> @ <price>  Limit order (IOC)\n"
              << "  " << grey(italic("[# <msgSeqNum>]")) << " " << blue_pastel(bold("mkt")) << " <b|s> <quantity>            Market order (IOC)\n"
              << "  " << grey(italic("[# <msgSeqNum>]")) << " " << blue_pastel(bold("cnl")) << " <b|s> <orderId>             Cancel order using the "
              << italic("clientOrderId") << " received from the server\n"
              << std::flush;
  }

  std::string _limit_order(std::string& fix, std::istringstream iss) {
    char at;
    char side;
    int64_t quantity;
    int64_t orderPrice;

    iss >> side >> quantity >> at >> orderPrice;
    fix += FIX::tag(FIX::ClOrdId_11, m_clOrdId++);
    fix += FIX::tag(FIX::Quantity_38, quantity);
    fix += FIX::tag(FIX::OrdType_40, FIX::OrdType_Limit);
    fix += FIX::tag(FIX::OrderPrice_44, orderPrice);
    fix += FIX::tag(FIX::Side_54, (side == 'b' ? FIX::Buy : FIX::Sell));
    fix += FIX::tag(FIX::TimeInForce_59, FIX::TimeInForce_GTC);

    return fix;
  }

  std::string _ioc_order(std::string& fix, std::istringstream&& iss) {
    char at;
    char side;
    int64_t quantity;
    int64_t orderPrice;

    iss >> side >> quantity >> at >> orderPrice;
    fix += FIX::tag(FIX::ClOrdId_11, m_clOrdId++);
    fix += FIX::tag(FIX::Quantity_38, quantity);
    fix += FIX::tag(FIX::OrdType_40, FIX::OrdType_Limit);
    fix += FIX::tag(FIX::OrderPrice_44, orderPrice);
    fix += FIX::tag(FIX::Side_54, (side == 'b' ? FIX::Buy : FIX::Sell));
    fix += FIX::tag(FIX::TimeInForce_59, FIX::TimeInForce_IOC);

    return fix;
  }

  std::string _mkt_order(std::string& fix, std::istringstream&& iss) {
    char side;
    int64_t quantity;

    iss >> side >> quantity;
    fix += FIX::tag(FIX::ClOrdId_11, m_clOrdId++);
    fix += FIX::tag(FIX::Quantity_38, quantity);
    fix += FIX::tag(FIX::OrdType_40, FIX::OrdType_Market);
    fix += FIX::tag(FIX::Side_54, (side == 'b' ? FIX::Buy : FIX::Sell));
    fix += FIX::tag(FIX::TimeInForce_59, FIX::TimeInForce_IOC);

    return fix;
  }

  std::string _cancel_order(std::string& fix, std::istringstream&& iss) {
    char side;
    int64_t clOrdId;

    iss >> side >> clOrdId;
    fix += FIX::tag(FIX::ClOrdId_11, m_clOrdId++);
    fix += FIX::tag(FIX::OrderId_37, clOrdId);
    fix += FIX::tag(FIX::OrigClOrdId_41, clOrdId);
    fix += FIX::tag(FIX::Side_54, (side == 'b' ? FIX::Buy : FIX::Sell));

    return fix;
  }

  void rx_thread() {
    udp::endpoint remote;
    std::array<char, 1024> buffer{};

    while (true) {
      std::size_t size = socket.receive_from(asio::buffer(buffer), remote);

      if (size == 0) {
        continue;
      }

      std::string message{buffer.data(), size};
      _process_message(std::move(message));
    }
  }

  void _process_message(std::string message) {
    const auto msgSeqNum = FIX::stol(FIX::tag(message, FIX::MsgSeqNum_34));
    const auto msgType = FIX::stoc(FIX::tag(message, FIX::MsgType_35));

    if (msgType == FIX::MsgType_ExecutionReport) {
      _process_execution_report(std::move(message));
    } else if (msgType == FIX::MsgType_CancelRejected) {
      _process_cancel_rejected(std::move(message));
    }
  }

  void _process_execution_report(std::string message) {
    const auto execType = FIX::stoc(FIX::tag(message, FIX::ExecType_150));

    if (execType == FIX::ExecType_New) {
      _process_new_order(std::move(message));
    } else if (execType == FIX::ExecType_Trade) {
      _process_trade(std::move(message));
    } else if (execType == FIX::ExecType_Canceled) {
      _process_canceled_order(std::move(message));
    }
  }

  void _process_new_order(std::string message) {
    const auto clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
    const auto orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
    const auto ordStatus = FIX::stoc(FIX::tag(message, FIX::OrdStatus_39));
    const auto orderPrice = FIX::stol(FIX::tag(message, FIX::OrderPrice_44));
    const auto side = FIX::stoc(FIX::tag(message, FIX::Side_54));
    const auto leavesQty = FIX::stol(FIX::tag(message, FIX::LeavesQty_151));

    message = std::format(
        "Order added: side={} clOrdId={} orderId={} leavesQty={} orderPrice={}\n", //
        (side == FIX::Buy ? "buy" : "sell"),
        clOrdId,
        orderId,
        leavesQty,
        orderPrice);

    std::cout << blue_pastel("    " + message) << std::flush;
  }

  void _process_trade(std::string message) {
    const auto ordStatus = FIX::stoc(FIX::tag(message, FIX::OrdStatus_39));
    const auto side = FIX::stoc(FIX::tag(message, FIX::Side_54));
    const auto clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
    const auto orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
    const auto orderPrice = FIX::stol(FIX::tag(message, FIX::OrderPrice_44));
    const auto cumQty = FIX::stol(FIX::tag(message, FIX::CumQty_14));
    const auto leavesQty = FIX::stol(FIX::tag(message, FIX::LeavesQty_151));
    const auto lastQty = FIX::stol(FIX::tag(message, FIX::LastQty_32));
    const auto lastPrice = FIX::stol(FIX::tag(message, FIX::LastPrice_31));

    message = std::format(
        "Order traded: side={} clOrdId={} orderId={} orderPrice={} lastPrice={} cumQty={} leavesQty={} lastQty={}\n", //
        (side == FIX::Buy ? "buy" : "sell"),
        clOrdId,
        orderId,
        orderPrice,
        lastPrice,
        cumQty,
        leavesQty,
        lastQty);

    std::cout << "    " << blue_pastel(message) << std::flush;
  }

  void _process_canceled_order(std::string message) {
    const auto clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
    const auto cumQty = FIX::stol(FIX::tag(message, FIX::CumQty_14));
    const auto orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
    const auto ordStatus = FIX::stoc(FIX::tag(message, FIX::OrdStatus_39));
    const auto origClOrdId = FIX::stol(FIX::tag(message, FIX::OrigClOrdId_41));
    const auto orderPrice = FIX::stol(FIX::tag(message, FIX::OrderPrice_44));
    const auto side = FIX::stoc(FIX::tag(message, FIX::Side_54));
    const auto leavesQty = FIX::stol(FIX::tag(message, FIX::LeavesQty_151));

    message = std::format(
        "Order canceled: clOrdId={} side={} orderPrice={} cumQty={} leavesQty={}\n", //
        clOrdId,
        (side == FIX::Buy ? "buy" : "sell"),
        orderPrice,
        cumQty,
        leavesQty);

    std::cout << "    " << blue_pastel(message) << std::flush;
  }

  void _process_cancel_rejected(std::string message) {
    const auto clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
    const auto orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
    const auto ordStatus = FIX::stoc(FIX::tag(message, FIX::OrdStatus_39));
    const auto origClOrdId = FIX::stol(FIX::tag(message, FIX::OrigClOrdId_41));
    const auto side = FIX::stoc(FIX::tag(message, FIX::Side_54));

    message = std::format(
        "Cancel rejected: clOrdId={} orderId={} origClOrdId={} ordStatus={} side={}\n", //
        clOrdId,
        orderId,
        origClOrdId,
        ordStatus,
        (side == FIX::Buy ? "buy" : "sell"));

    std::cout << "    " << red_pastel(message) << std::flush;
  }

public:

  int64_t m_clOrdId{1};
  int64_t m_msgSeqNum{1};
  asio::io_context io;
  udp::socket socket{io};
  udp::endpoint server_endpoint{asio::ip::make_address("127.0.0.1"), 9000};
};

int main(int argc, char* argv[]) {
  Client client;
  client.run();
}
