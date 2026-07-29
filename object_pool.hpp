/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "common.hpp"

template<typename TType>
class ObjectPool {

public: /* ctor, dtor */

  explicit ObjectPool(uint32_t capacity) noexcept
    : _capacity(capacity)
    , _size(0) //
  {
    assert(capacity <= 128 * 1024 * 1024);

    _buffer = (Byte*)(::operator new[](capacity * sizeof(TType), std::align_val_t{alignof(TType)}));
    _free = (TType**)(::operator new[](capacity * sizeof(TType*)));

    for(uint32_t i = 0; i < capacity; ++i) {
      _free[i] = (TType*)(_buffer + i * sizeof(TType));
    }
  }

  ~ObjectPool() noexcept{
    ::operator delete[](_buffer, std::align_val_t{alignof(TType)});
    ::operator delete[](_free);
  }

  ObjectPool(const ObjectPool&) = delete;
  ObjectPool& operator=(const ObjectPool&) = delete;

public: /* api */

  TType* allocate() noexcept {
    assert(full() == false);
    return _free[_size++];
  }

  void deallocate(TType* ptr) noexcept {
    assert(ptr >= (TType*)(_buffer));
    assert(ptr < (TType*)(_buffer + _capacity * sizeof(TType)));
    _free[--_size] = ptr;
  }


  template<typename... Args>
  TType* construct(Args&&... args) noexcept {
    return ::new((void*)(allocate())) TType(std::forward<Args>(args)...);
  }

  void destroy(TType* ptr) noexcept {
    ptr->~TType();
    deallocate(ptr);
  }

  TType& operator[](uint32_t index) noexcept {
    assert(index < _capacity);
    return *(TType*)(_buffer + index * sizeof(TType));
  }

  const TType& operator[](uint32_t index) const noexcept {
    assert(index < _capacity);
    return *reinterpret_cast<const TType*>(_buffer + index * sizeof(TType));
  }

  uint32_t index_of(const TType* ptr) const noexcept {
    assert((ptr >= (TType*)(_buffer)) && (ptr < (TType*)(_buffer + _capacity * sizeof(TType))));
    return (uint32_t)((const Byte*)(ptr) - _buffer) / sizeof(TType);
  }

  uint32_t capacity() const noexcept {
    return _capacity;
  }

  uint32_t size() const noexcept {
    return _size;
  }

  bool empty() const noexcept {
    return _size == 0;
  }

  bool full() const noexcept {
    return _size == _capacity;
  }

private: /* members */

  uint32_t _capacity;
  uint32_t _size;

  Byte* _buffer;
  TType** _free;
};