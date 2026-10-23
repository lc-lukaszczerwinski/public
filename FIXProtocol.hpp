/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "FIX.hpp"
#include "event.hpp"
#include "protocol.hpp"

struct FIXProtocol {
  static constexpr bool has_sequence_numbers = true;

  WireMessage encode_command(const OrderCommand& command) {
    std::string message;
    message += FIX::tag(FIX::MsgSeqNum_34, command.sequenceNumber);

    switch (command.type) {
    case OrderCommandType::LIMIT:
    case OrderCommandType::IOC:
    case OrderCommandType::MARKET:
      message += FIX::tag(FIX::MsgType_35, FIX::MsgType_NewOrderSingle);
      break;
    case OrderCommandType::CANCEL:
      message += FIX::tag(FIX::MsgType_35, FIX::MsgType_OrderCancelRequest);
      break;
    }

    message += FIX::tag(FIX::ClOrdId_11, command.clOrdId);

    if (command.type == OrderCommandType::CANCEL) {
      message += FIX::tag(FIX::OrderId_37, command.orderId);
      message += FIX::tag(FIX::OrigClOrdId_41, command.origClOrdId);
      message += FIX::tag(FIX::Side_54, command.side == Buy ? FIX::Buy : FIX::Sell);
      return message;
    }

    message += FIX::tag(FIX::Quantity_38, command.qty);
    message += FIX::tag(FIX::OrdType_40, command.type == OrderCommandType::MARKET ? FIX::OrdType_Market : FIX::OrdType_Limit);

    if (command.type != OrderCommandType::MARKET) {
      message += FIX::tag(FIX::OrderPrice_44, command.price);
    }

    message += FIX::tag(FIX::Side_54, command.side == Buy ? FIX::Buy : FIX::Sell);
    message += FIX::tag(FIX::TimeInForce_59, command.type == OrderCommandType::LIMIT ? FIX::TimeInForce_GTC : FIX::TimeInForce_IOC);
    return message;
  }

  std::optional<OrderCommand> decode_command(std::string message) {
    OrderCommand command{};
    command.sequenceNumber = FIX::stol(FIX::tag(message, FIX::MsgSeqNum_34));
    const auto msgType = FIX::stoc(FIX::tag(message, FIX::MsgType_35));

    if (msgType == FIX::MsgType_NewOrderSingle) {
      command.clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
      command.qty = FIX::stol(FIX::tag(message, FIX::Quantity_38));
      const auto orderType = FIX::stoc(FIX::tag(message, FIX::OrdType_40));

      if (orderType == FIX::OrdType_Limit) {
        command.price = FIX::stol(FIX::tag(message, FIX::OrderPrice_44));
      } else if (orderType == FIX::OrdType_Market) {
        command.price = 0;
      } else {
        return std::nullopt;
      }

      command.side = FIX::stoc(FIX::tag(message, FIX::Side_54)) == FIX::Buy ? Buy : Sell;
      const auto timeInForce = FIX::stoc(FIX::tag(message, FIX::TimeInForce_59));

      if (orderType == FIX::OrdType_Market) {
        command.type = OrderCommandType::MARKET;
      } else {
        command.type = timeInForce == FIX::TimeInForce_IOC ? OrderCommandType::IOC : OrderCommandType::LIMIT;
      }

      return command;
    }

    if (msgType == FIX::MsgType_OrderCancelRequest) {
      command.type = OrderCommandType::CANCEL;
      command.clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
      command.orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
      command.origClOrdId = FIX::stol(FIX::tag(message, FIX::OrigClOrdId_41));
      command.side = FIX::stoc(FIX::tag(message, FIX::Side_54)) == FIX::Buy ? Buy : Sell;
      return command;
    }

    return std::nullopt;
  }

  std::vector<WireMessage> encode_event(const Event& event) {
    if (event.type == Type::ORDER_ACCEPTED) {
      std::string message;
      message += FIX::tag(FIX::MsgSeqNum_34, _message_sequence_number++);
      message += FIX::tag(FIX::MsgType_35, FIX::MsgType_ExecutionReport);
      message += FIX::tag(FIX::ExecType_150, FIX::ExecType_New);
      message += FIX::tag(FIX::ClOrdId_11, event.makerId);
      message += FIX::tag(FIX::OrderId_37, event.makerSlot);
      message += FIX::tag(FIX::OrdStatus_39, FIX::OrdStatus_New);
      message += FIX::tag(FIX::OrderPrice_44, event.makerPrice);
      message += FIX::tag(FIX::Side_54, event.makerSide == Buy ? FIX::Buy : FIX::Sell);
      message += FIX::tag(FIX::LeavesQty_151, event.makerLeavesQty);
      return {std::move(message)};
    }

    else if (event.type == Type::ORDER_TRADED) {
      return {_encode_trade(event.takerId, event.takerSide, event.takerPrice, event.takerCumQty, event.takerLeavesQty, event.lastPrice, event.lastQty),
              _encode_trade(event.makerId, event.makerSide, event.makerPrice, event.makerCumQty, event.makerLeavesQty, event.lastPrice, event.lastQty)};
    }

    else if (event.type == Type::ORDER_CANCELED) {
      std::string message;
      message += FIX::tag(FIX::MsgSeqNum_34, _message_sequence_number++);
      message += FIX::tag(FIX::MsgType_35, FIX::MsgType_ExecutionReport);
      message += FIX::tag(FIX::ExecType_150, FIX::ExecType_Canceled);
      message += FIX::tag(FIX::ClOrdId_11, event.takerId);
      message += FIX::tag(FIX::CumQty_14, event.makerCumQty);
      message += FIX::tag(FIX::OrderId_37, event.makerSlot);
      message += FIX::tag(FIX::OrdStatus_39, FIX::OrdStatus_Canceled);
      message += FIX::tag(FIX::OrigClOrdId_41, event.makerId);
      message += FIX::tag(FIX::OrderPrice_44, event.makerPrice);
      message += FIX::tag(FIX::Side_54, event.makerSide == Buy ? FIX::Buy : FIX::Sell);
      message += FIX::tag(FIX::LeavesQty_151, 0);
      return {std::move(message)};
    }

    else if (event.type == Type::CANCEL_REJECTED) {
      std::string message;
      message += FIX::tag(FIX::MsgSeqNum_34, _message_sequence_number++);
      message += FIX::tag(FIX::MsgType_35, FIX::MsgType_CancelRejected);
      message += FIX::tag(FIX::ClOrdId_11, event.takerId);
      message += FIX::tag(FIX::OrderId_37, event.makerId);
      message += FIX::tag(FIX::OrdStatus_39, FIX::OrdStatus_Rejected);
      message += FIX::tag(FIX::OrigClOrdId_41, event.makerId);
      message += FIX::tag(FIX::Side_54, event.makerSide == Buy ? FIX::Buy : FIX::Sell);
      return {std::move(message)};
    }

    else {
      assert(false);
    }

    return {};
  }

  std::optional<OrderEvent> decode_event(std::string message) {
    OrderEvent event{};
    event.sequenceNumber = FIX::stol(FIX::tag(message, FIX::MsgSeqNum_34));
    const auto msgType = FIX::stoc(FIX::tag(message, FIX::MsgType_35));

    if (msgType == FIX::MsgType_CancelRejected) {
      event.type = OrderEventType::CANCEL_REJECTED;
      event.clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
      event.orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
      event.status = FIX::stoc(FIX::tag(message, FIX::OrdStatus_39));
      event.origClOrdId = FIX::stol(FIX::tag(message, FIX::OrigClOrdId_41));
      event.side = FIX::stoc(FIX::tag(message, FIX::Side_54)) == FIX::Buy ? Buy : Sell;
      return event;
    }

    if (msgType != FIX::MsgType_ExecutionReport) {
      return std::nullopt;
    }

    const auto execType = FIX::stoc(FIX::tag(message, FIX::ExecType_150));

    if (execType == FIX::ExecType_New) {
      event.type = OrderEventType::ACCEPTED;
      event.clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
      event.orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
      event.status = FIX::stoc(FIX::tag(message, FIX::OrdStatus_39));
      event.orderPrice = FIX::stol(FIX::tag(message, FIX::OrderPrice_44));
      event.side = FIX::stoc(FIX::tag(message, FIX::Side_54)) == FIX::Buy ? Buy : Sell;
      event.leavesQty = FIX::stol(FIX::tag(message, FIX::LeavesQty_151));
      return event;
    }

    else if (execType == FIX::ExecType_Trade) {
      event.type = OrderEventType::TRADED;
      event.status = FIX::stoc(FIX::tag(message, FIX::OrdStatus_39));
      event.side = FIX::stoc(FIX::tag(message, FIX::Side_54)) == FIX::Buy ? Buy : Sell;
      event.clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
      event.orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
      event.orderPrice = FIX::stol(FIX::tag(message, FIX::OrderPrice_44));
      event.cumQty = FIX::stol(FIX::tag(message, FIX::CumQty_14));
      event.leavesQty = FIX::stol(FIX::tag(message, FIX::LeavesQty_151));
      event.lastQty = FIX::stol(FIX::tag(message, FIX::LastQty_32));
      event.lastPrice = FIX::stol(FIX::tag(message, FIX::LastPrice_31));
      return event;
    }

    else if (execType == FIX::ExecType_Canceled) {
      event.type = OrderEventType::CANCELED;
      event.clOrdId = FIX::stol(FIX::tag(message, FIX::ClOrdId_11));
      event.cumQty = FIX::stol(FIX::tag(message, FIX::CumQty_14));
      event.orderId = FIX::stol(FIX::tag(message, FIX::OrderId_37));
      event.status = FIX::stoc(FIX::tag(message, FIX::OrdStatus_39));
      event.origClOrdId = FIX::stol(FIX::tag(message, FIX::OrigClOrdId_41));
      event.orderPrice = FIX::stol(FIX::tag(message, FIX::OrderPrice_44));
      event.side = FIX::stoc(FIX::tag(message, FIX::Side_54)) == FIX::Buy ? Buy : Sell;
      event.leavesQty = FIX::stol(FIX::tag(message, FIX::LeavesQty_151));
      return event;
    }

    else {
      assert(false);
    }

    return std::nullopt;
  }

private:

  std::string _encode_trade(Id id, Side side, Price orderPrice, Qty cumQty, Qty leavesQty, Price lastPrice, Qty lastQty) {
    std::string message;
    message += FIX::tag(FIX::MsgSeqNum_34, _message_sequence_number++);
    message += FIX::tag(FIX::MsgType_35, FIX::MsgType_ExecutionReport);
    message += FIX::tag(FIX::ExecType_150, FIX::ExecType_Trade);
    message += FIX::tag(FIX::OrdStatus_39, leavesQty == 0 ? FIX::OrdStatus_Filled : FIX::OrdStatus_PartiallyFilled);
    message += FIX::tag(FIX::Side_54, side == Buy ? FIX::Buy : FIX::Sell);
    message += FIX::tag(FIX::ClOrdId_11, id);
    message += FIX::tag(FIX::OrderId_37, id);
    message += FIX::tag(FIX::OrderPrice_44, orderPrice);
    message += FIX::tag(FIX::CumQty_14, cumQty);
    message += FIX::tag(FIX::LeavesQty_151, leavesQty);
    message += FIX::tag(FIX::LastQty_32, lastQty);
    message += FIX::tag(FIX::LastPrice_31, lastPrice);
    return message;
  }

  uint64_t _message_sequence_number{1};
};
