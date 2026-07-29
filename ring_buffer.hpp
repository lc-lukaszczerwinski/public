/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cstddef>
#include <queue>
#include <stdexcept>
#include <utility>

#include "common.hpp"

/*
 * RingBuffer
 */

template<typename TType, uint8_t TCapacity>
struct RingBuffer {

  static_assert((TCapacity > 0) && (TCapacity <= 128));

public: /* ctor, dtor */

  RingBuffer() noexcept
    : _head(0)
    , _tail(0) //
  {
  }

  RingBuffer(RingBuffer&&) = delete;
  RingBuffer(const RingBuffer&) = delete;

  RingBuffer& operator=(RingBuffer&&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

public: /* api */

  template<typename TT>
  bool push(TT&& t) noexcept {
    if(full()) {
      return false;
    }

    _buffer[_tail] = std::forward<TT>(t);
    _tail = _index(_tail + 1);

    return true;
  }

  bool pop(TType& out) noexcept {
    if(empty()) {
      return false;
    }

    out = std::move(_buffer[_head]);
    _head = _index(_head + 1);

    return true;
  }

  static constexpr uint8_t capacity() noexcept {
    return TCapacity;
  }

  [[nodiscard]] bool empty() const noexcept {
    return _head == _tail;
  }

  uint8_t size() const noexcept {
    if(_tail >= _head) {
      return _tail - _head;
    } else {
      return (TCapacity + 1) - (_head - _tail);
    }
  }

  bool full() const noexcept {
    return _index(_tail + 1) == _head;
  }

  /* extension */ TType _ext_pop() {
    TType out;

    if(! pop(out)) {
      throw std::runtime_error("RingBuffer is empty");
    }

    return out;
  }

  /* extension */ bool _ext_equal(std::queue<TType> expected) const noexcept {
    if(size() != expected.size()) {
      return false;
    }

    for(uint8_t i = _head; i != _tail; i = _index(i + 1)) {
      if(_buffer[i] != expected.front()) {
        return false;
      }

      expected.pop();
    }

    return expected.empty();
  }

private:

  static constexpr uint8_t _index(uint8_t i) noexcept {
    constexpr bool isPowerOf2 = ((TCapacity + 1) & TCapacity) == 0;

    if constexpr(isPowerOf2) {
      return i & TCapacity;
    } else {
      return (i == (TCapacity + 1) ? 0 : i); // return i % (TCapacity + 1);
    }
  }

private: /* members */

  uint8_t _head;
  uint8_t _tail;
  TType _buffer[TCapacity + /* N+1 trick */ 1];
};
