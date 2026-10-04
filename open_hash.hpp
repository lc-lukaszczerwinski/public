/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

template<typename TKey, typename TValue>
  requires(std::is_integral_v<TKey>)
struct FixedOpenHash {
public:

  constexpr static int32_t npos = -1;

public:

  explicit FixedOpenHash(int32_t capacity, float loadFactor = 0.5)
    : _capacity(capacity)
    , _step((capacity / 2) - 1)
    , _size(0) 
    , _loadFactor(loadFactor) //
  {
    assert(capacity >= 8);
    assert((capacity & (capacity - 1)) == 0);

    _keys = new TKey[_capacity];
    _values = new TValue[_capacity];

    const int32_t bits = (capacity + 7) / 8;

    _freeBits = new uint8_t[bits];
    _tombBits = new uint8_t[bits];

    std::memset(_freeBits, 0xFF, bits);
    std::memset(_tombBits, 0x00, bits);
  }

  ~FixedOpenHash() {
    delete[] _keys;
    delete[] _values;

    delete[] _freeBits;
    delete[] _tombBits;
  }

  FixedOpenHash(FixedOpenHash&&) = delete;
  FixedOpenHash(const FixedOpenHash&) = delete;

  FixedOpenHash& operator=(FixedOpenHash&&) = delete;
  FixedOpenHash& operator=(const FixedOpenHash&) = delete;

public:

  int32_t capacity() const noexcept {
    return _capacity;
  }

  int32_t size() const noexcept {
    return _size;
  }

public:

  bool insert(TKey key, const TValue& value) noexcept {
    if (_size >= _capacity * _loadFactor) {
      return false;
    }

    int32_t idx = key & (_capacity - 1);
    int32_t firstTomb = npos;

    for(int32_t iter = 0; iter < _capacity; ++iter) {
      const bool isFree = _get_bit(_freeBits, idx);
      const bool isTomb = _get_bit(_tombBits, idx);
      const bool isUsed = ! isFree && ! isTomb;

      if(isUsed && _keys[idx] == key) {
        _values[idx] = value;
        return true;
      }

      if(isTomb && firstTomb == npos) {
        firstTomb = idx;
      }

      if(isFree) {
        firstTomb = (firstTomb != npos) ? firstTomb : idx;
        break;
      }

      idx = (idx + _step) & (_capacity - 1);
    }

    if(firstTomb == npos) {
      return false;
    }

    _keys[firstTomb] = key;
    _values[firstTomb] = value;

    _clear_bit(_freeBits, firstTomb);
    _clear_bit(_tombBits, firstTomb);

    ++_size;

    return true;
  }

  TValue* find(TKey key) const noexcept {
    int32_t idx = key & (_capacity - 1);

    for(int32_t iter = 0; iter < _capacity; ++iter) {
      const bool isFree = _get_bit(_freeBits, idx);
      const bool isTomb = _get_bit(_tombBits, idx);
      const bool isUsed = ! isFree && ! isTomb;

      if(isUsed && _keys[idx] == key) {
        return &_values[idx];
      }

      if(isFree) {
        return nullptr;
      }

      idx = (idx + _step) & (_capacity - 1);
    }

    return nullptr;
  }

  TValue findOrDefault(TKey key, TValue defaultValue) const noexcept {
    int32_t idx = key & (_capacity - 1);

    for(int32_t iter = 0; iter < _capacity; ++iter) {
      const bool isFree = _get_bit(_freeBits, idx);
      const bool isTomb = _get_bit(_tombBits, idx);
      const bool isUsed = ! isFree && ! isTomb;

      if(isUsed && _keys[idx] == key) {
        return _values[idx];
      }

      if(isFree) {
        return defaultValue;
      }

      idx = (idx + _step) & (_capacity - 1);
    }

    return defaultValue;
  }

  bool erase(TKey key) noexcept {
    int32_t idx = key & (_capacity - 1);

    for(int32_t iter = 0; iter < _capacity; ++iter) {
      const bool isFree = _get_bit(_freeBits, idx);
      const bool isTomb = _get_bit(_tombBits, idx);
      const bool isUsed = ! isFree && ! isTomb;

      if(isUsed && _keys[idx] == key) {
        _set_bit(_tombBits, idx);
        --_size;
        return true;
      }

      if(isFree) {
        return false;
      }

      idx = (idx + _step) & (_capacity - 1);
    }

    return false;
  }

private:

  static bool _get_bit(const uint8_t* bits, int32_t idx) noexcept {
    return (bits[idx >> 3] >> (idx & 7)) & 1;
  }

  static void _set_bit(uint8_t* bits, int32_t idx) noexcept {
    bits[idx >> 3] |= uint8_t(1u << (idx & 7));
  }

  static void _clear_bit(uint8_t* bits, int32_t idx) noexcept {
    bits[idx >> 3] &= uint8_t(~(1u << (idx & 7)));
  }

private:

  int32_t _capacity;
  int32_t _step;
  int32_t _size;
  float _loadFactor;

  TKey* _keys;
  TValue* _values;

  uint8_t* _freeBits;
  uint8_t* _tombBits;
};

/*
 * OpenHash
 */

template<typename TKey, typename TValue>
struct OpenHash {
  struct Node {
    FixedOpenHash<TKey, TValue> hash;
    Node* next;

    Node(int32_t capacity)
      : hash(capacity)
      , next(nullptr) {
    }
  };

private:

  Node* _head;
  Node* _tail;

public:

  explicit OpenHash(int32_t initialCapacity = 1024) {
    _head = new Node(initialCapacity);
    _tail = _head;
  }

  ~OpenHash() {
    while(_head) {
      auto* next = _head->next;
      delete _head;
      _head = next;
    }
  }

  OpenHash(OpenHash&&) = delete;
  OpenHash(const OpenHash&) = delete;
 
  OpenHash& operator=(OpenHash&&) = delete;
  OpenHash& operator=(const OpenHash&) = delete;

  bool insert(TKey key, const TValue& value) {
    for(Node* node = _head; node; node = node->next) {
      const bool result = node->hash.insert(key, value);

      if(result) {
        return result;
      }
    }

    Node* next = new Node(2 * _tail->hash.capacity());
    _tail->next = next;
    _tail = next;

    return _tail->hash.insert(key, value);
  }

  TValue* find(TKey key) const {
    for(Node* node = _head; node; node = node->next) {
      TValue* found = node->hash.find(key);

      if(found != nullptr) {
        return found;
      }
    }

    return nullptr;
  }

  TValue findOrDefault(TKey key, TValue defaultValue) const {
    for(Node* node = _head; node; node = node->next) {
      TValue found = node->hash.findOrDefault(key, defaultValue);

      if(found != defaultValue) {
        return found;
      }
    }

    return defaultValue;
  }

  bool erase(TKey key) {
    for(Node* node = _head; node; node = node->next) {
      if(node->hash.erase(key)) {
        return true;
      }
    }

    return false;
  }
};
