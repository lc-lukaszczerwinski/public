#pragma once

/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include <atomic>
#include <cstddef>
#include <queue>
#include <stdexcept>
#include <utility>

#include "common.hpp"

/*
 * Circular Buffer, Single Producer Single Consumer, Lock Free
 */

template<typename TType, uint8_t Capacity>
struct RingBufferSPSC {

  static_assert(std::is_nothrow_copy_constructible_v<TType>);
  static_assert(std::is_nothrow_copy_assignable_v<TType>);
  static_assert(std::is_default_constructible_v<TType>);

  static_assert((Capacity > 0) && (Capacity <= 128));
  static_assert(std::atomic<uint8_t>::is_always_lock_free);

public: // types

  using value_type = TType;

public: // static

  static constexpr uint8_t capacity() noexcept{
    return Capacity;
  }

public: // ctor, dtor

  RingBufferSPSC() noexcept {
  }

  RingBufferSPSC(RingBufferSPSC&&) = delete;
  RingBufferSPSC(const RingBufferSPSC&) = delete;

  RingBufferSPSC& operator=(RingBufferSPSC&&) = delete;
  RingBufferSPSC& operator=(const RingBufferSPSC&) = delete;

public: // api  

  template<typename T>
  bool push(T&& value) noexcept {
    const uint8_t tail = _tail.load(std::memory_order_relaxed);
    const uint8_t next = _index(tail + 1);

    if(UNLIKELY(next == _headCached)) {
      _headCached = _head.load(std::memory_order_acquire);

      if(UNLIKELY(next == _headCached)) {
        return false;
      }
    }

    _buffer[tail] = std::forward<T>(value);
    _tail.store(next, std::memory_order_release);

    return true;
  }

  bool pop(TType& out) noexcept {
    const uint8_t head = _head.load(std::memory_order_relaxed);

    if(UNLIKELY(head == _tailCached)) {
      _tailCached = _tail.load(std::memory_order_acquire);

      if(UNLIKELY(head == _tailCached)) {
        return false;
      }
    }

    out = std::move(_buffer[head]);
    _head.store(_index(head + 1), std::memory_order_release);

    return true;
  }

  /* extension */ TType _ext_pop() {
    TType out;

    if(pop(out) == false) {
      throw std::runtime_error("Buffer is empty");
    }

    return out;
  }

public: // approx

  uint8_t _approx_size() const noexcept {
    const uint8_t head = _head.load(std::memory_order_acquire);
    const uint8_t tail = _tail.load(std::memory_order_acquire);

    if(tail >= head) {
      return tail - head;
    } else {
      return (Capacity + 1) - (head - tail);
    }
  }

  bool _approx_empty() const noexcept {
    const uint8_t head = _head.load(std::memory_order_acquire);
    const uint8_t tail = _tail.load(std::memory_order_acquire);

    return head == tail;
  }

  bool _approx_full() const noexcept{
    const uint8_t head = _head.load(std::memory_order_acquire);
    const uint8_t tail = _tail.load(std::memory_order_acquire);

    return head == _index(tail + 1);
  }

public: // debug

  /* debug */ bool _debug_equal(std::queue<TType> expected) const noexcept {
    if (_approx_size() != expected.size()) {
      return false;
    }

    const uint8_t head = _head.load(std::memory_order_acquire);
    const uint8_t tail = _tail.load(std::memory_order_acquire);

    for(uint8_t i = head; i != tail; i = _index(i + 1)) {
      if(_buffer[i] != expected.front()) {
        return false;
      }

      expected.pop();
    }

    return expected.empty();
  }

private: // static

  static constexpr uint8_t _index(uint8_t i) noexcept {
    constexpr bool isPowerOf2 = ((Capacity + 1) & Capacity) == 0;

    if constexpr(isPowerOf2) {
      return i & Capacity;
    } else {
      return (i == (Capacity + 1) ? 0 : i); // return i % (Capacity + 1);
    }
  }

private: // members

  // producer line, writes _tail, uses _headCached
  alignas(CacheLineSize) std::atomic<uint8_t> _tail{0};
                         uint8_t _headCached{0};

  // consumer line, writes _head, uses _tailCached
  alignas(CacheLineSize) std::atomic<uint8_t> _head{0};
                         uint8_t _tailCached{0};

  alignas(CacheLineSize) TType _buffer[Capacity + /* N+1 trick */ 1];
};
