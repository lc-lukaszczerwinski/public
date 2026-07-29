/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include "flat_list.hpp"

#include <cassert>
#include <list>
#include <format>

#include "test_utils.hpp"

namespace {

void test_empty_state() {
	FlatList<int, 5> list;

	assert(list.size() == 0);
	assert(list.empty());
	assert(list.full() == false);
}

void test_push_front_and_back() {
	FlatList<int, 5> list;

	list.push_front(20);
	assert(list._ext_equal({20}));
	assert(list.front() == 20);
	assert(list.back() == 20);

	const int8_t pos30 = list.push_back(30);
	assert(list._ext_equal({20, 30}));
	assert(list.front() == 20);
	assert(list.back() == 30);

	list.push_front(10);
	assert(list._ext_equal({10, 20, 30}));

	list.push_back(40);
	assert(list._ext_equal({10, 20, 30, 40}));

	list[pos30] = 35;
	assert(list._ext_equal({10, 20, 35, 40}));
	assert(list.size() == 4);
}

void test_pop_front_and_back() {
	FlatList<int, 5> list;

	list.push_back(10);
	list.push_back(20);
	list.push_back(30);
	list.push_back(40);
	assert(list._ext_equal({10, 20, 30, 40}));

	list.pop_front();
	assert(list._ext_equal({20, 30, 40}));
	assert(list.front() == 20);

	list.pop_back();
	assert(list._ext_equal({20, 30}));
	assert(list.back() == 30);

	list.pop_front();
	assert(list._ext_equal({30}));

	list.pop_back();
	assert(list._ext_equal({}));
	assert(list.empty());
}

void test_insert_before_head_and_at_end() {
	FlatList<int, 5> list;

	const int8_t pos20 = list.push_back(20);
	list.push_back(40);
	assert(list._ext_equal({20, 40}));

	list.insert(pos20, 10);
	assert(list._ext_equal({10, 20, 40}));
	assert(list.front() == 10);

	list.insert(/* push_back */ -1, 50);
	assert(list._ext_equal({10, 20, 40, 50}));
	assert(list.back() == 50);
}

void test_insert_middle() {
	FlatList<int, 5> list;

	list.push_back(10);
	const int8_t pos30 = list.push_back(30);
	const int8_t pos50 =list.push_back(50);
	assert(list._ext_equal({10, 30, 50}));

	list.insert(pos30, 20);
	assert(list._ext_equal({10, 20, 30, 50}));

	list.insert(pos50, 40);
	assert(list._ext_equal({10, 20, 30, 40, 50}));
	assert(list.full());
}

void test_erase_head_tail_and_middle() {
	FlatList<int, 5> list;

	list.push_back(10);
	list.push_back(20);
	list.push_back(30);
	list.push_back(40);
	list.push_back(50);
	assert(list._ext_equal({10, 20, 30, 40, 50}));

	list.erase(0);
	assert(list._ext_equal({20, 30, 40, 50}));
	assert(list.front() == 20);

	list.erase(4);
	assert(list._ext_equal({20, 30, 40}));
	assert(list.back() == 40);

	list.erase(2);
	assert(list._ext_equal({20, 40}));
	assert(list.size() == 2);
}

void test_reuse_erased_indexes() {
	FlatList<int, 5> list;

	list.push_back(10);
	list.push_back(20);
	list.push_back(30);
	list.push_back(40);
	assert(list._ext_equal({10, 20, 30, 40}));

	list.erase(1);
	assert(list._ext_equal({10, 30, 40}));

	list.insert(2, 25);
	assert(list._ext_equal({10, 25, 30, 40}));

	list.pop_back();
	assert(list._ext_equal({10, 25, 30}));

	list.push_back(45);
	assert(list._ext_equal({10, 25, 30, 45}));

	list.pop_front();
	assert(list._ext_equal({25, 30, 45}));

	list.push_front(5);
	assert(list._ext_equal({5, 25, 30, 45}));
}

void test_mixed_operations() {
	FlatList<int, 6> list;

	const int8_t pos20 = list.push_back(20);
	const int8_t pos10 = list.push_front(10);
	const int8_t pos40 = list.push_back(40);
	assert(list._ext_equal({10, 20, 40}));

	list.insert(pos40, 30);
	assert(list._ext_equal({10, 20, 30, 40}));

	list.erase(pos20);
	assert(list._ext_equal({10, 30, 40}));

	list.insert(/* push_back */ -1, 50);
	assert(list._ext_equal({10, 30, 40, 50}));

	list.erase(pos10);
	assert(list._ext_equal({30, 40, 50}));

	list.push_front(25);
	assert(list._ext_equal({25, 30, 40, 50}));

	assert(list.front() == 25);
	assert(list.back() == 50);
	assert(list.size() == 4);
}

template<std::size_t Capacity>
void bench_push_back(const std::string& label = " :: ") {

  struct Benchmark {
    Benchmark() {
    }

    FlatList<int, Capacity> _list;

    void setup() {
    }

    void run() {
      for(int i = 0; i < _list.capacity(); ++i) {
        _list.push_back(i);
				do_not_optimize(_list);
			}
    }

    void teardown() {
      while(!_list.empty()) {
        _list.pop_front();
      }
    }
  } bench;

  Timer<8>(bench).log([iters = Capacity, label](int ns, const std::string& msg) {
    std::cout << std::format("{} :: {} :: {} iters :: {} ns/total :: {:.1f} ns/iter :: {} iter/s\n",
			PROFILE, label, iters, ns, (1.0 * ns / iters), (int)(1e9 * iters / ns)) << std::flush;
  });
};

template<std::size_t Capacity>
void bench_pop_front(const std::string& label = " :: ") {

  struct Benchmark {
    Benchmark() {
    }

    FlatList<int, Capacity> _list;

    void setup() {
      for(int i = 0; i < _list.capacity(); ++i) {
        _list.push_back(i);
      }
    }

    void run() {
      while(!_list.empty()) {
        _list.pop_front();
				do_not_optimize(_list);
      }
    }

    void teardown() {
    }
  } bench;

  Timer<8>(bench).log([iters = Capacity, label](int ns, const std::string& msg) {
    std::cout << std::format("{} :: {} :: {} iters :: {} ns/total :: {:.1f} ns/iter :: {} iter/s\n",
			PROFILE, label, iters, ns, (1.0 * ns / iters), (int)(1e9 * iters / ns)) << std::flush;
  });
};

} // namespace

int main() {

#ifdef HFT_DEBUG
	test_empty_state();
	test_push_front_and_back();
	test_pop_front_and_back();
	test_insert_before_head_and_at_end();
	test_insert_middle();
	test_erase_head_tail_and_middle();
	test_reuse_erased_indexes();
	test_mixed_operations();
#endif

  bench_push_back<128>("FlatList::push_back");
  bench_pop_front<128>("FlatList::pop_front");
}
