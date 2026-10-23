/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <limits>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "OUCH.hpp"
#include "event.hpp"
#include "protocol.hpp"

struct OUCHProtocol {
  /* 
   * Ideally OUCH protocol should be used with SoupBinTCP so sequence numbers are handled by the transport
   */

  static constexpr bool has_sequence_numbers = false;

  WireMessage encode_command(const OrderCommand& command) {
    OUCH::Writer writer;

    if (command.type == OrderCommandType::CANCEL) {
      _cancel_sides[command.origClOrdId] = command.side;
      writer.u8(OUCH::MsgType_CancelOrder);
      writer.i64(command.origClOrdId);
      writer.u32(0);
      return writer.finish();
    }

    assert(command.qty >= MinQty && command.qty <= MaxQty);
    assert(command.price >= 0 && command.price <= MaxPrice);
    assert(command.type != OrderCommandType::LIMIT || command.price >= MinPrice);

    writer.u8(OUCH::MsgType_EnterOrder);
    writer.i64(command.clOrdId);
    writer.u8(command.side == Buy ? OUCH::Buy : OUCH::Sell);
    writer.u32(command.qty);
    writer.string(OUCH::Stock, OUCH::StockLength);
    writer.u32(command.price);
    writer.u32(command.type == OrderCommandType::LIMIT //
                   ? OUCH::TimeInForce_GTC
                   : OUCH::TimeInForce_IOC);

    _orders[command.clOrdId] = OrderState{.side = command.side, //
                                          .price = command.price,
                                          .leavesQty = command.qty};

    return writer.finish();
  }

  std::optional<OrderCommand> decode_command(std::string message) {
    bool invalid = false;
    OUCH::Reader reader{message};
    const auto messageType = reader.u8();

    if (messageType == OUCH::MsgType_EnterOrder) {
      const auto tokenOpt = reader.i64();
      const auto sideOpt = reader.u8();
      const auto quantityOpt = reader.u32();
      const auto stockOpt = reader.string(OUCH::StockLength);
      const auto priceOpt = reader.u32();
      const auto timeInForceOpt = reader.u32();

      invalid = ! tokenOpt;
      invalid = invalid || ! sideOpt;
      invalid = invalid || ! quantityOpt;
      invalid = invalid || ! stockOpt;
      invalid = invalid || ! priceOpt;
      invalid = invalid || ! timeInForceOpt;
      invalid = invalid || ! reader.empty();
      invalid = invalid || (stockOpt) && (*stockOpt != OUCH::Stock);

      if (invalid) {
        return std::nullopt;
      }

      invalid = (*sideOpt != OUCH::Buy) && (*sideOpt != OUCH::Sell);

      if (invalid) {
        return std::nullopt;
      }

      invalid = *quantityOpt < (uint32_t)(MinQty);
      invalid = invalid || *quantityOpt > (uint32_t)(MaxQty);
      invalid = invalid || *priceOpt > (uint32_t)(MaxPrice);
      invalid = invalid || (*timeInForceOpt != OUCH::TimeInForce_GTC) && (*timeInForceOpt != OUCH::TimeInForce_IOC);

      if (invalid) {
        return std::nullopt;
      }

      OrderCommandType type;

      if (*timeInForceOpt == OUCH::TimeInForce_IOC && *priceOpt == 0) {
        type = OrderCommandType::MARKET;
      } else {
        if (*timeInForceOpt == OUCH::TimeInForce_IOC) {
          type = OrderCommandType::IOC;
        } else {
          type = OrderCommandType::LIMIT;
        }
      }

      OrderCommand command{.type = type, //
                           .side = *sideOpt == OUCH::Buy ? Buy : Sell,
                           .clOrdId = *tokenOpt,
                           .qty = (Qty)*quantityOpt,
                           .price = (Price)*priceOpt};

      _orders[command.clOrdId] = OrderState{.side = command.side, //
                                            .price = command.price,
                                            .leavesQty = command.qty};

      return command;
    }

    else if (messageType == OUCH::MsgType_CancelOrder) {
      const auto tokenOpt = reader.i64();
      const auto quantityOpt = reader.u32();

      invalid = ! tokenOpt;
      invalid = invalid || ! quantityOpt;
      invalid = invalid || ! reader.empty();

      if (invalid) {
        return std::nullopt;
      }

      invalid = (*quantityOpt != 0) && (*quantityOpt > (uint32_t)(MaxQty));

      if (invalid) {
        return std::nullopt;
      }

      return OrderCommand{.type = OrderCommandType::CANCEL, //
                          .side = _order_side(*tokenOpt),
                          .clOrdId = *tokenOpt,
                          .orderId = *tokenOpt,
                          .origClOrdId = *tokenOpt,
                          .qty = (Qty)*quantityOpt};
    }

    else {
      assert(false);
    }

    return std::nullopt;
  }

  std::vector<WireMessage> encode_event(const Event& event) {
    if (event.type == Type::ORDER_ACCEPTED) {
      OUCH::Writer writer;
      writer.u8(OUCH::MsgType_Accepted);
      writer.i64(event.makerId);
      writer.u64((uint64_t)(event.makerId));
      writer.u8(event.makerSide == Buy ? OUCH::Buy : OUCH::Sell);
      writer.u32((uint32_t)(event.makerLeavesQty));
      writer.string(OUCH::Stock, OUCH::StockLength);
      writer.u32((uint32_t)(event.makerPrice));
      return {writer.finish()};
    }

    else if (event.type == Type::ORDER_TRADED) {
      return {_encode_trade(event.takerId, event.takerCumQty, event.takerLeavesQty, event.lastPrice, event.lastQty),
              _encode_trade(event.makerId, event.makerCumQty, event.makerLeavesQty, event.lastPrice, event.lastQty)};
    }

    else if (event.type == Type::ORDER_CANCELED) {
      OUCH::Writer writer;
      writer.u8(OUCH::MsgType_Canceled);
      writer.i64(event.makerId);

      auto state = _orders.find(event.makerId);
      const Qty canceledQty = state == _orders.end() //
                                  ? 0
                                  : state->second.leavesQty;

      writer.u32((uint32_t)(canceledQty));
      writer.u8('U');
      _orders.erase(event.makerId);
      return {writer.finish()};
    }

    else if (event.type == Type::CANCEL_REJECTED) {
      OUCH::Writer writer;
      writer.u8(OUCH::MsgType_Rejected);
      writer.i64(event.makerId);
      writer.u8(OUCH::RejectionReason_Cancel);
      return {writer.finish()};
    }

    else {
      assert(false);
    }

    return {};
  }

  std::optional<OrderEvent> decode_event(std::string message) {
    bool invalid = false;
    OUCH::Reader reader{message};
    const auto messageType = reader.u8();

    if (messageType == OUCH::MsgType_Accepted) {
      const auto tokenOpt = reader.i64();
      const auto orderReferenceOpt = reader.u64();
      const auto sideOpt = reader.u8();
      const auto quantityOpt = reader.u32();
      const auto stockOpt = reader.string(OUCH::StockLength);
      const auto priceOpt = reader.u32();

      invalid = ! tokenOpt;
      invalid = invalid || ! orderReferenceOpt;
      invalid = invalid || ! sideOpt;
      invalid = invalid || ! quantityOpt;
      invalid = invalid || ! stockOpt;
      invalid = invalid || ! priceOpt;
      invalid = invalid || ! reader.empty();
      invalid = invalid || (stockOpt && *stockOpt != OUCH::Stock);

      if (invalid) {
        return std::nullopt;
      }

      invalid = (*sideOpt != OUCH::Buy && *sideOpt != OUCH::Sell);
      invalid = invalid || *orderReferenceOpt > (uint64_t)(std::numeric_limits<Id>::max());
      invalid = invalid || *quantityOpt > (uint32_t)(MaxQty);
      invalid = invalid || *priceOpt > (uint32_t)(MaxPrice);

      if (invalid) {
        return std::nullopt;
      }

      auto state = _orders.find(*tokenOpt);

      if (state != _orders.end()) {
        state->second.orderReference = *orderReferenceOpt;
        state->second.leavesQty = *quantityOpt;
      }

      return OrderEvent{.type = OrderEventType::ACCEPTED,
                        .side = *sideOpt == OUCH::Buy ? Buy : Sell,
                        .clOrdId = *tokenOpt,
                        .orderId = (Id)*orderReferenceOpt,
                        .orderPrice = (Price)*priceOpt,
                        .leavesQty = (Qty)*quantityOpt,
                        .status = '0'};
    }

    if (messageType == OUCH::MsgType_Executed) {
      const auto tokenOpt = reader.i64();
      const auto orderReferenceOpt = reader.u64();
      const auto quantityOpt = reader.u32();
      const auto priceOpt = reader.u32();

      invalid = ! tokenOpt;
      invalid = invalid || ! orderReferenceOpt;
      invalid = invalid || ! quantityOpt;
      invalid = invalid || ! priceOpt;
      invalid = invalid || ! reader.empty();

      if (invalid) {
        return std::nullopt;
      }

      invalid = *orderReferenceOpt > (uint64_t)(std::numeric_limits<Id>::max());
      invalid = invalid || *quantityOpt > (uint32_t)(MaxQty);
      invalid = invalid || *priceOpt > (uint32_t)(MaxPrice);

      if (invalid) {
        return std::nullopt;
      }

      auto state = _orders.find(*tokenOpt);
      Qty cumQty = (Qty)(*quantityOpt);
      Qty leavesQty = 0;
      Side side{};
      Price orderPrice = (Price)(*priceOpt);

      if (state != _orders.end()) {
        if ((Qty)(*quantityOpt) > state->second.leavesQty) {
          return std::nullopt;
        }

        state->second.cumQty += *quantityOpt;
        state->second.leavesQty -= *quantityOpt;
        cumQty = state->second.cumQty;
        leavesQty = state->second.leavesQty;
        side = state->second.side;
        orderPrice = state->second.price;
      }

      return OrderEvent{.type = OrderEventType::TRADED,
                        .side = side,
                        .clOrdId = *tokenOpt,
                        .orderId = (Id)*orderReferenceOpt,
                        .orderPrice = orderPrice,
                        .cumQty = cumQty,
                        .leavesQty = leavesQty,
                        .lastPrice = (Price)*priceOpt,
                        .lastQty = (Qty)*quantityOpt,
                        .status = leavesQty == 0 ? '2' : '1'};
    }

    if (messageType == OUCH::MsgType_Canceled) {
      const auto tokenOpt = reader.i64();
      const auto quantityOpt = reader.u32();
      const auto reasonOpt = reader.u8();

      invalid = ! tokenOpt;
      invalid = invalid || ! quantityOpt;
      invalid = invalid || ! reasonOpt;
      invalid = invalid || ! reader.empty();

      if (invalid) {
        return std::nullopt;
      }

      invalid = false;
      invalid = invalid || *quantityOpt > (uint32_t)(MaxQty);

      if (invalid) {
        return std::nullopt;
      }

      auto state = _orders.find(*tokenOpt);
      OrderEvent event{.type = OrderEventType::CANCELED, //
                       .clOrdId = *tokenOpt,
                       .orderId = *tokenOpt,
                       .origClOrdId = *tokenOpt,
                       .leavesQty = 0,
                       .status = '4'};

      if (state != _orders.end()) {
        if ((Qty)(*quantityOpt) > state->second.leavesQty) {
          return std::nullopt;
        }

        event.side = state->second.side;
        event.orderPrice = state->second.price;
        event.orderId = state->second.orderReference;
        event.cumQty = state->second.cumQty;
        event.leavesQty = state->second.leavesQty - (Qty)(*quantityOpt);
        _orders.erase(state);
      }

      _cancel_sides.erase(*tokenOpt);
      return event;
    }

    if (messageType == OUCH::MsgType_Rejected) {
      const auto tokenOpt = reader.i64();
      const auto reasonOpt = reader.u8();

      invalid = ! tokenOpt;
      invalid = invalid || ! reasonOpt;
      invalid = invalid || ! reader.empty();

      if (invalid) {
        return std::nullopt;
      }

      const auto cancelSide = _cancel_sides.find(*tokenOpt);
      const Side side = cancelSide == _cancel_sides.end() ? _order_side(*tokenOpt) : cancelSide->second;
      _cancel_sides.erase(*tokenOpt);

      return OrderEvent{.type = OrderEventType::CANCEL_REJECTED, //
                        .side = side,
                        .clOrdId = *tokenOpt,
                        .orderId = *tokenOpt,
                        .origClOrdId = *tokenOpt,
                        .status = *reasonOpt};
    }

    return std::nullopt;
  }

private:

  struct OrderState {
    Side side{};
    Id orderReference{};
    Price price{};
    Qty cumQty{};
    Qty leavesQty{};
  };

  Side _order_side(Id i64) const {
    const auto state = _orders.find(i64);
    return state == _orders.end() ? Sell : state->second.side;
  }

  std::string _encode_trade(Id id, Qty cumQty, Qty leavesQty, Price lastPrice, Qty lastQty) {
    OUCH::Writer writer;
    writer.u8(OUCH::MsgType_Executed);
    writer.i64(id);
    writer.u64(id);
    writer.u32(lastQty);
    writer.u32(lastPrice);
    auto state = _orders.find(id);

    if (state != _orders.end()) {
      state->second.cumQty = cumQty;
      state->second.leavesQty = leavesQty;
    }

    return writer.finish();
  }

  std::map<Id, OrderState> _orders;
  std::map<Id, Side> _cancel_sides;
};
