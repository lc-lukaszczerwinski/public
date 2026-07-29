/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <list>
#include <type_traits>

#include "common.hpp"

/*
 * IntrusiveListNode
 */

template<typename /* CRTP */ TType>
struct IntrusiveListNode {

  TType* _M_prev = nullptr;
  TType* _M_next = nullptr;

  IntrusiveListNode() noexcept
    : _M_prev(nullptr)
    , _M_next(nullptr) //
  {}

  IntrusiveListNode(const IntrusiveListNode&) noexcept
    : _M_prev(nullptr)
    , _M_next(nullptr) //
  {}

  IntrusiveListNode(IntrusiveListNode&&) noexcept
    : _M_prev(nullptr)
    , _M_next(nullptr) //
  {}

  IntrusiveListNode& operator=(const IntrusiveListNode&) noexcept {
    _M_prev = nullptr;
    _M_next = nullptr;
    return *this;
  }

  IntrusiveListNode& operator=(IntrusiveListNode&&) noexcept {
    _M_prev = nullptr;
    _M_next = nullptr;
    return *this;
  }
};

/*
 * FlatList
 */

template<typename TNode>
struct IntrusiveList {

  static_assert(std::is_base_of_v<IntrusiveListNode<TNode>, TNode>);

public: /* ctod, dtor */

  IntrusiveList() noexcept
    : _head(nullptr)
    , _tail(nullptr)
  {
  }

  IntrusiveList(IntrusiveList&&) = delete;
  IntrusiveList(const IntrusiveList&) = delete;

  ~IntrusiveList() noexcept {
    assert(empty());
  }

  IntrusiveList& operator=(IntrusiveList&&) = delete;
  IntrusiveList& operator=(const IntrusiveList&) = delete;

public: /* api */

  void push_front(TNode& node) noexcept {
    assert(node._M_prev == nullptr && node._M_next == nullptr);

    TNode* next = _head;
    node._M_prev = nullptr;
    node._M_next = next;

    if(next != nullptr) {
      next->_M_prev = &node;
    } else {
      _tail = &node;
    }
   
    _head = &node;
  }

  void push_back(TNode& node) noexcept {
    assert(node._M_prev == nullptr && node._M_next == nullptr);

    TNode* prev = _tail;
    node._M_prev = prev;
    node._M_next = nullptr;

    if(prev != nullptr) {
      prev->_M_next = &node;
    } else {
      _head = &node;
    }

    _tail = &node;
  }

  void pop_front() noexcept {
    assert(empty() == false);

    TNode* node = _head;
    TNode* next = _head->_M_next;
   
    if(next != nullptr) {
      next->_M_prev = nullptr;
    } else {
      _tail = nullptr;
    }
   
    _head = next;

    node->_M_prev = nullptr;
    node->_M_next = nullptr;
  }

  void pop_back() noexcept {
    assert(empty() == false);

    TNode* prev = _tail->_M_prev;
    TNode* node = _tail;

    if(prev != nullptr) {
      prev->_M_next = nullptr;
    } else {
      _head = nullptr;
    }

    _tail = prev;

    node->_M_prev = nullptr;
    node->_M_next = nullptr;
  }

  void insert(TNode* next, TNode& node) noexcept {
    if(UNLIKELY(_head == next)) {
      return push_front(node);
    }

    if(UNLIKELY(nullptr == next)) {
      return push_back(node);
    }

    assert(node._M_prev == nullptr && node._M_next == nullptr);

    TNode* prev = next->_M_prev;
    node._M_prev = prev;
    node._M_next = next;
    next->_M_prev = &node;
    prev->_M_next = &node;
  }

  void erase(TNode& node) noexcept {
    if(UNLIKELY(_head == &node)) {
      return pop_front();
    }

    if(UNLIKELY(_tail == &node)) {
      return pop_back();
    }

    assert(empty() == false);

    TNode* const prev = node._M_prev;
    TNode* const next = node._M_next;

    prev->_M_next = next;
    next->_M_prev = prev;
  }

  TNode& front() noexcept {
    assert(empty() == false);
    return *_head;
  }

  const TNode& front() const noexcept {
    assert(empty() == false);
    return *_head;
  }

  TNode& back() noexcept {
    assert(empty() == false);
    return *_tail;
  }

  const TNode& back() const noexcept {
    assert(empty() == false);
    return *_tail;
  }

  [[nodiscard]] bool empty() const noexcept {
    return (_head == nullptr);
  }

public: /* extension */

  bool _ext_equal(std::list<TNode> /* copy */ expected) const noexcept {
    for(TNode* node = _head; (node != nullptr) && (expected.empty() == false); node = node->_M_next) {
      assert((node->_M_prev == nullptr) || (node->_M_prev->_M_next == node));
      assert((node->_M_next == nullptr) || (node->_M_next->_M_prev == node));


      if(*node != expected.front()) {
        return false;
      }

      expected.pop_front();
    }

    return expected.empty();
  }

private: /* members */

  TNode* _head;
  TNode* _tail;
};
