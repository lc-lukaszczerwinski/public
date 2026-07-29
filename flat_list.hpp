#pragma once

/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include <cassert>
#include <cstdint>
#include <list>
#include <type_traits>

#include "flat_object_pool.hpp"

/*
 * FlatList
 */

template<typename TType, uint8_t TCapacity>
struct FlatList {
  static_assert(std::is_nothrow_copy_constructible_v<TType>);
  static_assert(std::is_nothrow_copy_assignable_v<TType>);
  static_assert(std::is_default_constructible_v<TType>);

  static_assert((TCapacity > 0) && (TCapacity <= 128));

public: /* const */

  constexpr static uint8_t None = (uint8_t)-1;

public: /* static */

  static constexpr uint8_t capacity() noexcept {
    return TCapacity;
  }

public: /* ctor, dtor */

  FlatList() noexcept
    : _head(None)
    , _tail(None) //
  {
  }

  FlatList(FlatList&&) = delete;
  FlatList(const FlatList&) = delete;

  FlatList& operator=(FlatList&&) = delete;
  FlatList& operator=(const FlatList&) = delete;

public: /* api */

  uint8_t push_front(const TType& value) noexcept {
    assert(_pool.full() == false);

    const uint8_t next = _head;
    const uint8_t index = _pool.allocate();

    _Node& node = _pool[index];
    node._prev = None;
    node._next = next;
    node._value = value;

    if(next != None) {
      _pool[next]._prev = index;
    } else {
      _tail = index;
    }

    _head = index;
    return index;
  }

  uint8_t push_back(const TType& value) noexcept {
    assert(_pool.full() == false);

    const uint8_t prev = _tail;
    const uint8_t index = _pool.allocate();

    _Node& node = _pool[index];
    node._prev = prev;
    node._next = None;
    node._value = value;

    if(prev != None) {
      _pool[prev]._next = index;
    } else {
      _head = index;
    }

    _tail = index;
    return index;
  }

  void pop_front() noexcept {
    assert(_pool.empty() == false);

    const uint8_t prev = _head;
    const uint8_t next = _pool[_head]._next;

    if(next != None) {
      _pool[next]._prev = None;
    } else {
      _tail = None;
    }

    _head = next;
    _pool.deallocate(prev);
  }

  void pop_back() noexcept {
    assert(_pool.empty() == false);

    const uint8_t prev = _pool[_tail]._prev;
    const uint8_t next = _tail;

    if(prev != None) {
      _pool[prev]._next = None;
    } else {
      _head = None;
    }

    _tail = prev;
    _pool.deallocate(next);
  }

  uint8_t insert(uint8_t next /* None */, const TType& value) noexcept {
    if(UNLIKELY(next == _head)) {
      return push_front(value);
    }

    if(UNLIKELY(next == None)) {
      return push_back(value);
    }

    assert(_pool.full() == false);

    const uint8_t prev = _pool[next]._prev;
    const uint8_t index = _pool.allocate();

    _Node& node = _pool[index];
    node._prev = prev;
    node._next = next;
    node._value = value;

    _pool[next]._prev = index;
    _pool[prev]._next = index;

    return index;
  }

  void erase(uint8_t index) noexcept {
    if(UNLIKELY(index == _head)) {
      return pop_front();
    }

    if(UNLIKELY(index == _tail)) {
      return pop_back();
    }

    assert(_pool.empty() == false);

    const uint8_t prev = _pool[index]._prev;
    const uint8_t next = _pool[index]._next;

    _pool[prev]._next = next;
    _pool[next]._prev = prev;

    _pool.deallocate(index);
  }

  TType& front() noexcept {
    assert(_pool.empty() == false);
    return _pool[_head]._value;
  }

  const TType& front() const noexcept {
    assert(_pool.empty() == false);
    return _pool[_head]._value;
  }

  TType& back() noexcept {
    assert(_pool.empty() == false);
    return _pool[_tail]._value;
  }

  const TType& back() const noexcept {
    assert(_pool.empty() == false);
    return _pool[_tail]._value;
  }

  TType& operator[](uint8_t index) noexcept {
    return _pool[index]._value;
  }

  const TType& operator[](uint8_t index) const noexcept {
    return _pool[index]._value;
  }

  uint8_t size() const noexcept {
    return _pool.size();
  }

  bool empty() const noexcept {
    return _pool.empty();
  }

  bool full() const noexcept {
    return _pool.full();
  }

public: /* extension */

  bool _ext_equal(std::list<TType> /* copy */ expected) const noexcept {
    if(this->size() != expected.size()) {
      return false;
    }

    for(uint8_t index = _head; index != None; index = _pool[index]._next) {
      assert((_pool[index]._prev == None) || (_pool[_pool[index]._prev]._next == index));
      assert((_pool[index]._next == None) || (_pool[_pool[index]._next]._prev == index));

      if(_pool[index]._value != expected.front()) {
        return false;
      }

      expected.pop_front();
    }

    return true;
  }

private: /* types */

  struct _Node {
    uint8_t _prev;
    uint8_t _next;
    TType _value;
  };

private: /* members */

  uint8_t _head;
  uint8_t _tail;
  FlatObjectPool<_Node, TCapacity> _pool;
};
