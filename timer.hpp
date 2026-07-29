/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <string>

#include <x86intrin.h>

struct Duration {
  using duration = std::chrono::duration<long int, std::nano>;

  Duration(const duration& d, uint64_t cycles = 0)
    : _ns(std::chrono::duration_cast<std::chrono::nanoseconds>(d).count())
    , _cycles(cycles) {
  }

  Duration(long int ns, uint64_t cycles)
    : _ns(ns)
    , _cycles(cycles) {
  }

  operator long int() const {
    return ns();
  }

  long int ns() const {
    return _ns;
  }

  long int us() const {
    return ns() / 1000;
  }

  long int ms() const {
    return ns() / 1000 / 1000;
  }

  long int s() const {
    return ns() / 1000 / 1000 / 1000;
  }

  uint64_t cycles() const {
    return _cycles;
  }

  std::string format() const {
    if(ns() < 1000) {
      return std::to_string(ns()) + "ns";
    } else if(us() < 1000) {
      return std::to_string(us()) + "µs";
    } else if(ms() < 1000) {
      return std::to_string(ms()) + "ms";
    } else {
      return std::to_string(s()) + "s";
    }
  }

  void log(std::function<void(long int, const std::string&)> f) const {
    f(ns(), format());
  }

  long int _ns;
  uint64_t _cycles;
};

template<unsigned Iters = 8>
struct _Timer {
  template<typename F>
  Duration operator()(F& f) {
    {
      /*
          * Warm up
          */

      for(int i = 1; i < Iters; ++i) {
        f.setup();
        f.run();
        f.teardown();
      }
    }

    unsigned aux = 0u;
    long int bestNs = std::numeric_limits<long int>::max();
    uint64_t bestCycles = std::numeric_limits<uint64_t>::max();

    for(int i = 0; i < Iters; ++i) {
      for(int j = 0; j < Iters; ++j) {
        f.setup();

        const auto startTime = std::chrono::high_resolution_clock::now();
        const uint64_t startCycles = __rdtscp(&aux);
        asm volatile("" ::: "memory");

        f.run();

        asm volatile("" ::: "memory");
        const uint64_t endCycles = __rdtscp(&aux);
        const auto endTime = std::chrono::high_resolution_clock::now();

        f.teardown();

        const long int runNs = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count();
        const uint64_t runCycles = endCycles - startCycles;

        if(runNs < bestNs) {
          bestNs = runNs;
          bestCycles = runCycles;
        }
      }
    }

    return Duration(bestNs, bestCycles);
  }
};

template<unsigned Iters>
inline static _Timer<Iters> Timer;
