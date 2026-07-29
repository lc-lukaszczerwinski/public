/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <functional>
#include <type_traits>

#include "common.hpp"

/*
 * IntrusiveTreapNode
 */

template<typename /* CRTP */ TType>
struct IntrusiveTreapNode {

  TType* _M_left = nullptr;
  TType* _M_right = nullptr;
  TType* _M_parent = nullptr;

  IntrusiveTreapNode() noexcept
    : _M_left(nullptr)
    , _M_right(nullptr)
    , _M_parent(nullptr) //
  {
  }

  ~IntrusiveTreapNode() noexcept {
    _M_left = nullptr;
    _M_right = nullptr;
    _M_parent = nullptr;
  }

  IntrusiveTreapNode(const IntrusiveTreapNode&) noexcept
    : _M_left(nullptr)
    , _M_right(nullptr)
    , _M_parent(nullptr) //
  {
  }

  IntrusiveTreapNode(IntrusiveTreapNode&&) noexcept
    : _M_left(nullptr)
    , _M_right(nullptr)
    , _M_parent(nullptr) //
  {
  }

  IntrusiveTreapNode& operator=(const IntrusiveTreapNode&) noexcept {
    _M_left = nullptr;
    _M_right = nullptr;
    _M_parent = nullptr;
    return *this;
  }

  IntrusiveTreapNode& operator=(IntrusiveTreapNode&&) noexcept {
    _M_left = nullptr;
    _M_right = nullptr;
    _M_parent = nullptr;
    return *this;
  }

  TType* _prev() noexcept {
    if(this->_M_left != nullptr) {
      IntrusiveTreapNode* current = this->_M_left;

      while(current->_M_right != nullptr) {
        current = current->_M_right;
      }

      return (TType*)current;
    }

    IntrusiveTreapNode* current = this;
    IntrusiveTreapNode* parent = current->_M_parent;

    while(parent != nullptr && parent->_M_left == current) {
      current = parent;
      parent = parent->_M_parent;
    }

    return (TType*)parent;
  }

  TType* _next() noexcept {
    if(this->_M_right != nullptr) {
      IntrusiveTreapNode* current = this->_M_right;

      while(current->_M_left != nullptr) {
        current = current->_M_left;
      }

      return (TType*)current;
    }

    IntrusiveTreapNode* current = this;
    IntrusiveTreapNode* parent = current->_M_parent;

    while(parent != nullptr && parent->_M_right == current) {
      current = parent;
      parent = parent->_M_parent;
    }

    return (TType*)parent;
  }
};

/*
 * IntrusiveTreap
 */

template<typename TNode, typename Cmp = std::less<typename TNode::key_type>>
struct IntrusiveTreap {

  static_assert(std::is_base_of_v<IntrusiveTreapNode<TNode>, TNode>);

public: /* ctor, dtor */

  IntrusiveTreap() noexcept
    : _root(nullptr) //
  {
  }

  IntrusiveTreap(const IntrusiveTreap&) = delete;
  IntrusiveTreap(IntrusiveTreap&&) = delete;

  ~IntrusiveTreap() noexcept {
    assert(empty());
  }

  IntrusiveTreap& operator=(const IntrusiveTreap&) = delete;
  IntrusiveTreap& operator=(IntrusiveTreap&&) = delete;

public: /* api */

  template<typename TKey>
  TNode* find(const TKey& key, TNode*& parent) noexcept {
    TNode* current = _root;
    parent = nullptr;

    while(current != nullptr) {
      parent = current;

      if(Cmp()(key, current->_get_key())) {
        current = current->_M_left;
      } else if(Cmp()(current->_get_key(), key)) {
        current = current->_M_right;
      } else {
        return current;
      }
    }

    return nullptr;
  }

  template<typename TKey>
  TNode* find(const TKey& key) noexcept {
    TNode* parent;
    return find(key, parent);
  }

  void insert(TNode& node, TNode* parent) noexcept {
    assert(node._M_left == nullptr);
    assert(node._M_right == nullptr);
    assert(node._M_parent == nullptr);

    node._M_parent = parent;

    if(UNLIKELY(parent == nullptr)) {
      _root = &node;
    } else if(Cmp()(node._get_key(), parent->_get_key())) {
      assert(parent->_M_left == nullptr);
      parent->_M_left = &node;
    } else {
      assert(parent->_M_right == nullptr);
      parent->_M_right = &node;
    }

    _bubble_up(&node);
  }

  bool insert(TNode& node) noexcept {
    TNode* parent;
    TNode* const found = find(node._get_key(), parent);

    if(UNLIKELY(found != nullptr)) {
      return false;
    }

    insert(node, parent);
    return true;
  }

  void erase(TNode& node) noexcept {
    while(node._M_left != nullptr || node._M_right != nullptr) {
      if(node._M_left == nullptr) {
        _rotate_left(&node);
      } else if(node._M_right == nullptr) {
        _rotate_right(&node);
      } else {
        if(node._M_left->_get_priority() > node._M_right->_get_priority()) {
          _rotate_right(&node);
        } else {
          _rotate_left(&node);
        }
      }
    }

    TNode* const parent = node._M_parent;

    if(UNLIKELY(parent == nullptr)) {
      _root = nullptr;
    } else if(parent->_M_left == &node) {
      parent->_M_left = nullptr;
    } else {
      parent->_M_right = nullptr;
    }

    node._M_parent = nullptr;
    node._M_left = nullptr;
    node._M_right = nullptr;
  }

  TNode* front() noexcept {
    TNode* const node = _root;

    if(node == nullptr) {
      return nullptr;
    }

    while(node->_M_left != nullptr) {
      node = node->_M_left;
    }

    return node;
  }

  const TNode* front() const noexcept {
    return const_cast<IntrusiveTreap*>(this)->front();
  }

  TNode* back() noexcept {
    TNode* const node = _root;

    if(node == nullptr) {
      return nullptr;
    }

    while(node->_M_right != nullptr) {
      node = node->_M_right;
    }

    return node;
  }

  const TNode* back() const noexcept {
    return const_cast<IntrusiveTreap*>(this)->back();
  }

  [[nodiscard]] bool empty() const noexcept {
    return (_root == nullptr);
  }

private: /* rotations */

  void _rotate_left(TNode* node) noexcept {
    TNode* right = node->_M_right;
    assert(right != nullptr);

    TNode* const parent = node->_M_parent;
    node->_M_right = right->_M_left;

    if(right->_M_left != nullptr) {
      right->_M_left->_M_parent = node;
    }

    right->_M_left = node;
    node->_M_parent = right;
    right->_M_parent = parent;

    if(UNLIKELY(parent == nullptr)) {
      _root = right;
    } else if(parent->_M_left == node) {
      parent->_M_left = right;
    } else {
      parent->_M_right = right;
    }
  }

  void _rotate_right(TNode* node) noexcept {
    TNode* left = node->_M_left;
    assert(left != nullptr);

    TNode* const parent = node->_M_parent;
    node->_M_left = left->_M_right;

    if(left->_M_right != nullptr) {
      left->_M_right->_M_parent = node;
    }

    left->_M_right = node;
    node->_M_parent = left;
    left->_M_parent = parent;

    if(UNLIKELY(parent == nullptr)) {
      _root = left;
    } else if(parent->_M_left == node) {
      parent->_M_left = left;
    } else {
      parent->_M_right = left;
    }
  }

private: /* heap fix-up */

  void _bubble_up(TNode* node) noexcept {
    while((node->_M_parent != nullptr) &&                            //
          (node->_get_priority() > node->_M_parent->_get_priority()) //
    ) {
      TNode* parent = node->_M_parent;

      if(parent->_M_left == node) {
        _rotate_right(parent);
      } else {
        _rotate_left(parent);
      }
    }
  }

private: /* members */

  TNode* _root;
};

/*
 * Intrusive-Cached-Treap : `front` and `back` are cached for O(1) access
 */

template<typename TNode, typename Cmp = std::less<typename TNode::key_type>>
struct IntrusiveCTreap {

  IntrusiveTreap<TNode, Cmp> _impl;

  static_assert(std::is_base_of_v<IntrusiveTreapNode<TNode>, TNode>);

public: /* ctor, dtor */

  IntrusiveCTreap() noexcept
    : _impl()
    , _cache_front(nullptr)
    , _cache_back(nullptr) //
  {
  }

  IntrusiveCTreap(const IntrusiveCTreap&) = delete;
  IntrusiveCTreap(IntrusiveCTreap&&) = delete;

  ~IntrusiveCTreap() noexcept {
  }

  IntrusiveCTreap& operator=(const IntrusiveCTreap&) = delete;
  IntrusiveCTreap& operator=(IntrusiveCTreap&&) = delete;

public: /* api */

  template<typename TKey>
  TNode* find(const TKey& key, TNode*& parent) noexcept {
    return _impl.find(key, parent);
  }

  template<typename TKey>
  TNode* find(const TKey& key) noexcept {
    return _impl.find(key);
  }

  void insert(TNode& node, TNode* parent) noexcept {
    _impl.insert(node, parent);
    _cache_update_on_insert(&node);
  }

  bool insert(TNode& node) noexcept {
    const bool result = _impl.insert(node);
    _cache_update_on_insert(&node);
    return result;
  }

  void erase(TNode& node) noexcept {
    _cache_update_on_erase(&node);
    _impl.erase(node);
  }

  TNode* front() noexcept {
    return _cache_front;
  }

  const TNode* front() const noexcept {
    return const_cast<IntrusiveCTreap*>(this)->front();
  }

  TNode* back() noexcept {
    return _cache_back;
  }

  const TNode* back() const noexcept {
    return const_cast<IntrusiveCTreap*>(this)->back();
  }

  [[nodiscard]] bool empty() const noexcept {
    return _impl.empty();
  }

private: /* extension */

  void _cache_update_on_insert(TNode* node) noexcept {
    if(UNLIKELY(_cache_front == nullptr)) {
      _cache_front = node;
      _cache_back = node;
      return;
    }

    if(UNLIKELY(Cmp()(node->_get_key(), _cache_front->_get_key()))) {
      _cache_front = node;
    }

    if(UNLIKELY(Cmp()(_cache_back->_get_key(), node->_get_key()))) {
      _cache_back = node;
    }
  }

  void _cache_update_on_erase(TNode* node) noexcept {
    if(UNLIKELY(node == _cache_front)) {
      _cache_front = node->_next();
    }

    if(UNLIKELY(node == _cache_back)) {
      _cache_back = node->_prev();
    }
  }

  TNode* _cache_front;
  TNode* _cache_back;
};
