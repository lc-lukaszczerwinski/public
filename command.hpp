/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include <cassert>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>

#include "common.hpp"

#pragma once

struct Command {
  Id seq;
  Id order;
  Price price; // {price!=0 : limit}, {price=0 : market}
  Qty qty;   // {qty>0 : buy}, {qty<0 : sell}, {qty=0, price>0 : cancel buy}, {qty=0, price<0 : cancel sell}
};

std::string to_string(const Command& command) {
  std::ostringstream os;

  std::string type;

  if(command.price == 0 && command.qty > 0) {
    type = "Market side:Buy";
  } else if(command.price == 0 && command.qty < 0) {
    type = "Market side:Sell";
  } else if(command.price > 0 && command.qty > 0) {
    type = "Limit side:Buy";
  } else if(command.price > 0 && command.qty < 0) {
    type = "Limit side:Sell";
  } else if(command.price < 0 && command.qty == 0) {
    type = "Cancel side:Sell";
  } else if(command.price > 0 && command.qty == 0) {
    type = "Cancel side:Buy";
  } else {
    assert(false);
  }

  os << "Command: seq=" << command.seq << " type=" << type << " id=" << command.order << " price=" << std::abs(command.price) << " qty=" << std::abs(command.qty);

  return os.str();
}

std::ostream& operator<<(std::ostream& os, const Command& command) {
  return os << to_string(command);
}
