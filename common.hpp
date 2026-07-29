/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cxxabi.h>
#include <limits>
#include <sstream>
#include <string>

#ifdef NDEBUG
  #define HFT_RELEASE
#else
  #define HFT_DEBUG
#endif


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

typedef std::byte Byte;
typedef int32_t Id;
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

static constexpr Index InvalidOrderId = 0;

// std::hardware_destructive_interference_size
inline constexpr std::size_t CacheLineSize = 64;

/*
 * Profile
 */

#ifdef NDEBUG
constexpr auto PROFILE = "Release";
#else
constexpr auto PROFILE = "Debug";
#endif

/*
 * do_not_optimize prevents the compiler from optimizing away the given value.
 */

template<typename T>
inline void do_not_optimize(const T& value) {
  asm volatile("" : : "g"(value) : "memory");
}

