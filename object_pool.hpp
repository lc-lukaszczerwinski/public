/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>
#include <type_traits>
#include <utility>

#include "common.hpp"

template<typename TType>
class ObjectFixedPool {

public: /* ctor, dtor */

  explicit ObjectFixedPool(uint32_t capacity) noexcept
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

  ~ObjectFixedPool() noexcept {
    ::operator delete[](_buffer, std::align_val_t{alignof(TType)});
    ::operator delete[](_free);
  }

  ObjectFixedPool(ObjectFixedPool&&) = delete;
  ObjectFixedPool(const ObjectFixedPool&) = delete;

  ObjectFixedPool& operator=(ObjectFixedPool&&) = delete;
  ObjectFixedPool& operator=(const ObjectFixedPool&) = delete;

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
    assert(ptr != nullptr);
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
    return (uint32_t)((const Byte*)(ptr)-_buffer) / sizeof(TType);
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

/*
 * ObjectPool
 */

template<typename TType, uint32_t ChunkSize = 1024u>
class ObjectPool {

  static_assert(ChunkSize > 0);

public: /* ctor, dtor */

  ObjectPool() noexcept {
    if(_allocateChunk() == false) {
      std::terminate();
    }
  }

  ~ObjectPool() noexcept {
    Chunk* chunk = _chunks;

    while(chunk != nullptr) {
      Chunk* next = chunk->next;
      ::operator delete(chunk, std::align_val_t{alignof(Chunk)});
      chunk = next;
    }
  }

  ObjectPool(ObjectPool&&) = delete;
  ObjectPool(const ObjectPool&) = delete;

  ObjectPool& operator=(ObjectPool&&) = delete;
  ObjectPool& operator=(const ObjectPool&) = delete;

public: /* api */

  TType* allocate() noexcept {
    if(_free == nullptr) {
      if(_allocateChunk() == false) {
        return nullptr;
      }
    }

    const Slot* slot = _free;
    _free = slot->next;

    return (TType*)(void*)(&slot->storage);
  }

  void deallocate(TType* object) noexcept {
    if(object == nullptr) {
      return;
    }

    Slot* slot = reinterpret_cast<Slot*>(object);
    slot->next = _free;
    _free = slot;
  }

  template<typename... Args>
  TType* construct(Args&&... args) noexcept {
    const TType* ptr = allocate();

    if (ptr != nullptr) {
      return ::new((void*)ptr) TType(std::forward<Args>(args)...);
    } else {
      return nullptr;
    }
  }

  void destroy(TType* ptr) noexcept {
    assert(ptr != nullptr);
    ptr->~TType();
    deallocate(ptr);
  }

private: /* types */

  union Slot {
    Slot* next;
    std::aligned_storage_t<sizeof(TType), alignof(TType)> storage;
  };

  struct Chunk {
    Chunk* next;
    Slot slots[ChunkSize];
  };

private: /* methods */

  bool _allocateChunk() noexcept {
    Chunk* const chunk = (Chunk*)(::operator new(sizeof(Chunk),  //
                                                 std::align_val_t{alignof(Chunk)}, 
                                                 std::nothrow));

    if(chunk == nullptr) {
      return false;
    }

    chunk->next = _chunks;
    _chunks = chunk;

    for(uint32_t i = 0; i < ChunkSize; ++i) {
      chunk->slots[i].next = _free;
      _free = &chunk->slots[i];
    }

    return true;
  }

private: /* members */

  Chunk* _chunks{nullptr};
  Slot* _free{nullptr};
};
