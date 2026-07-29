/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cstdint>

#ifdef NDEBUG
  #define HFT_RELEASE
#else
  #define HFT_DEBUG
#endif

#ifdef HFT_DEBUG
  constexpr auto PROFILE = "Debug";
#else
  constexpr auto PROFILE = "Release";
#endif

/*
 * do_not_optimize
 */

template<typename T>
inline void do_not_optimize(const T& value) {
  asm volatile("" : : "g"(value) : "memory");
}

/*
 * Branch prediction hints
 */

// #define LIKELY(x) (__builtin_expect(! ! (x), 1))
// #define UNLIKELY(x) (__builtin_expect(! ! (x), 0))

#define LIKELY(x) (x)
#define UNLIKELY(x) (x)

/*
 * types
 */

typedef uint8_t Byte;
typedef int64_t Id;
typedef int32_t Qty;
typedef int32_t Price;
typedef int32_t Index;

/*
 * Side
 */

typedef int8_t Side;
constexpr Side Sell = -1;
constexpr Side Buy = 1;

/*
 * Constants
 */

static constexpr Price MinPrice = 1;
static constexpr Price MaxPrice = 128 * 1024 * 1024;

static constexpr Qty MinQty = 1;
static constexpr Qty MaxQty = 64 * 1024 * 1024;
