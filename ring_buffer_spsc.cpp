/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include "ring_buffer_spsc.hpp"

#include <atomic>
#include <format>
#include <thread>

#include "test_utils.hpp"

namespace {

template<int32_t Capacity>
void test_empty_and_full_state() {
	RingBufferSPSC<int, Capacity> actual;

	assert(actual._approx_empty());
	assert(actual._approx_size() == 0);
	assert(actual._approx_full() == false);

	int out = -1;
	assert(actual.pop(out) == false);

	for(int32_t i = 0; i < Capacity; i += 1) {
		assert(actual.push(i));
		assert(actual._approx_size() == static_cast<std::size_t>(i + 1));
	}

	assert(actual._approx_empty() == false);
	assert(actual._approx_full());
	assert(actual.push(999) == false);

	for(int32_t i = 0; i < Capacity; i += 1) {
		assert(actual.pop(out));
		assert(out == i);
	}

	assert(actual._approx_empty());
	assert(actual._approx_size() == 0);
	assert(actual._approx_full() == false);
	assert(actual.pop(out) == false);
}

template<int32_t Capacity>
void test_push_pop() {
	RingBufferSPSC<int, Capacity> actual;
	std::queue<int> expected;

	for(int32_t i = 0; i < Capacity / 2; i += 1) {
		assert(actual.push(i));
		expected.push(i);
		assert(actual._debug_equal(expected));
		assert(actual._approx_size() == expected.size());
	}

	for(int32_t i = 0; i < Capacity * 3; i += 1) {
		int out = -1;

		assert(actual.push(i + 100));
		expected.push(i + 100);
		assert(actual._debug_equal(expected));

		assert(actual.pop(out));
		assert(out == expected.front());
		expected.pop();
		assert(actual._debug_equal(expected));
	}

	while(! expected.empty()) {
		int out = -1;

		assert(actual.pop(out));
		assert(out == expected.front());
		expected.pop();
		assert(actual._debug_equal(expected));
	}

	assert(actual._approx_empty());
}

template<int32_t Capacity>
void test_wrap_around_and_reuse() {
	RingBufferSPSC<int, Capacity> actual;
	std::queue<int> expected;

	for(int32_t cycle = 0; cycle < Capacity * 4; cycle += 1) {
		while(expected.size() < static_cast<std::size_t>(Capacity)) {
			const int value = cycle * 10 + static_cast<int>(expected.size());

			assert(actual.push(value));
			expected.push(value);
			assert(actual._debug_equal(expected));
		}

		assert(actual._approx_full());
		assert(actual.push(-1) == false);

		const int pops = (cycle % Capacity) + 1;

		for(int32_t i = 0; i < pops; i += 1) {
			int out = -1;

			assert(actual.pop(out));
			assert(out == expected.front());
			expected.pop();
			assert(actual._debug_equal(expected));
		}
	}

	while(! expected.empty()) {
		int out = -1;

		assert(actual.pop(out));
		assert(out == expected.front());
		expected.pop();
	}

	assert(actual._approx_empty());
}

void test_ext_pop_throws() {
	RingBufferSPSC<int, 4> actual;
	bool thrown = false;

	try {
		(void)actual._ext_pop();
	} catch(const std::runtime_error&) {
		thrown = true;
	}

	assert(thrown);

	assert(actual.push(10));
	assert(actual.push(20));
	assert(actual._ext_pop() == 10);
	assert(actual._ext_pop() == 20);
}

template<int32_t Capacity, int32_t Iterations>
void test_stress_spsc_two_threads() {
	RingBufferSPSC<int, Capacity> actual;
	std::atomic<bool> start{false};
	std::atomic<bool> done{false};
	std::atomic<int32_t> consumedCount{0};

	std::thread producer([&]() {
		while(start.load(std::memory_order_acquire) == false) {
			std::this_thread::yield();
		}

		for(int32_t i = 0; i < Iterations; ) {
			if(actual.push(i)) {
				i += 1;
			} else {
				std::this_thread::yield();
			}
		}

		done.store(true, std::memory_order_release);
	});

	std::thread consumer([&]() {
		while(start.load(std::memory_order_acquire) == false) {
			std::this_thread::yield();
		}

		int32_t expected = 0;

		while(expected < Iterations) {
			int32_t out = -1;

			if(actual.pop(out)) {
				assert(out == expected);
				expected += 1;
				consumedCount.store(expected, std::memory_order_release);
			} else if(done.load(std::memory_order_acquire) == false) {
				std::this_thread::yield();
			}
		}
	});

	start.store(true, std::memory_order_release);

	producer.join();
	consumer.join();

	assert(consumedCount.load(std::memory_order_acquire) == Iterations);
	assert(actual._approx_empty());
}

template<std::size_t Capacity>
void bench_push(const std::string& label) {

  struct Benchmark {
    Benchmark() {
    }

    RingBufferSPSC<int, Capacity> _buffer;

    void setup() {
    }

    void run() {
      for(int i = 0; i < _buffer.capacity(); ++i) {
        _buffer.push(i);
				do_not_optimize(_buffer);
			}
    }

    void teardown() {
      while(!_buffer._approx_empty()) {
        _buffer._ext_pop();
      }
    }
  } bench;

  Timer<1>(bench).log([iters = Capacity, label](int ns, const std::string& msg) {
    std::cout << std::format("{} :: {} :: {} iters :: {} ns/total :: {:.1f} ns/iter :: {} iter/s\n",
			PROFILE, label, iters, ns, (1.0 * ns / iters), (int)(1e9 * iters / ns)) << std::flush;
  });
};

template<std::size_t Capacity>
void bench_pop(const std::string& label) {

  struct Benchmark {
    Benchmark() {
    }

    RingBufferSPSC<int, Capacity> _buffer;

    void setup() {
      for(int i = 0; i < _buffer.capacity(); ++i) {
        _buffer.push(i);
      }
    }

    void run() {
      while(!_buffer._approx_empty()) {
        _buffer._ext_pop();
				do_not_optimize(_buffer);
      }
    }

    void teardown() {
    }
  } bench;

  Timer<1>(bench).log([iters = Capacity, label](int ns, const std::string& msg) {
    std::cout << std::format("{} :: {} :: {} iters :: {} ns/total :: {:.1f} ns/iter :: {} iter/s\n",
			PROFILE, label, iters, ns, (1.0 * ns / iters), (int)(1e9 * iters / ns)) << std::flush;
  });
};

} // namespace

/*
 * main
 */

int main() {

#ifdef HFT_DEBUG
	test_empty_and_full_state<1>();
	test_empty_and_full_state<8>();
	test_empty_and_full_state<15>();
	test_empty_and_full_state<16>();
	test_empty_and_full_state<17>();

	test_push_pop<1>();
	test_push_pop<8>();
	test_push_pop<15>();
	test_push_pop<16>();
	test_push_pop<17>();


	test_wrap_around_and_reuse<1>();
	test_wrap_around_and_reuse<8>();
	test_wrap_around_and_reuse<15>();
	test_wrap_around_and_reuse<16>();
	test_wrap_around_and_reuse<17>();

	test_ext_pop_throws();

	test_stress_spsc_two_threads<1, 200000>();
	test_stress_spsc_two_threads<8, 200000>();
	test_stress_spsc_two_threads<17, 200000>();
#endif

  bench_push<128>("RingBufferSPSC::push");
  bench_pop<128>("RingBufferSPSC::pop");

	return 0;
}
