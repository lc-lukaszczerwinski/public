/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <limits>

/*
 * Linear Congruential Generator
 * By Hull-Dobell Theorem, the period is 2^32 for all seed values that are coprime to 2^32.
 */

struct LCG {
  using result_type = uint32_t;

  static constexpr uint32_t min() noexcept {
    return std::numeric_limits<uint32_t>::min();
  }

  static constexpr uint32_t max() noexcept {
    return std::numeric_limits<uint32_t>::max();
  }

  LCG(uint32_t seed = 7) noexcept
    : state(seed) {
    assert(seed & 1);
  }

  uint32_t operator()() noexcept {
    return (state = state * 1664525u + 1013904223u);
  }

private:

  uint32_t state;
};
