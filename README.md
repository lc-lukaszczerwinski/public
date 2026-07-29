
# LC++ Łukasz Czerwiński offers

### __Data Structures & Memory Architecture__  
Design of custom data structures, allocators, and memory layouts optimized for locality, cache efficiency, and deterministic performance.
  
### __Algorithms & Performance Engineering__  
Performance-focused algorithm design with emphasis on low latency, predictable behavior, and bottleneck elimination.

### __Matching Engine & Market Infrastructure__  
Architecture and implementation of matching engines, order-processing pipelines, and market-data systems for real-time environments.

### __Consulting & Technical Advisory__  
Technical consulting for performance-critical systems, architecture modernization, and engineering decision support.

## DEMO

[![Build](https://github.com/lc-lukaszczerwinski/public/actions/workflows/build.yml/badge.svg)](https://github.com/lc-lukaszczerwinski/public/actions/workflows/build.yml)

**order_book_v3** - a minimalistic order book with zero external dependencies. Supports limit orders, market orders, immediate-or-cancel (IOC), insert, quantity/price update and cancellation, producing events in just a few nanoseconds. Built as a treap for price priority and intrusive FIFO lists for time priority providing deterministic bahaviour and O(1) order updates and cancellations via OrderId.
  
```cpp
  Event event;
  EventHandler eh;
  OrderBook oBook(eh);

  oBook.template insert_order<Sell /* -1 */>(/* orderId */ 1, /* price */ 100, /* qty */ 10);
  event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.maker == 1);
  assert(event.side == Sell);
  assert(event.price == price);
  assert(event.qty == 10);

  oBook.template insert_order<Buy /* +1 */>(/* orderId */ 2, /* price */ 101, /* qty */ 5);
  event = oBook.eh().pop();
  assert(event.type == ORDER_ACK);
  assert(event.maker == 2);
  assert(event.side == Buy);
  assert(event.price == price);
  assert(event.qty == 5);

  event = oBook.eh().pop();
  assert(event.type == TRADE);
  assert(event.maker == 1);
  assert(event.taker == 2);
  assert(event.price == price);
  assert(event.qty == 5);
```

### Benchmark Methodology

- Platform: Intel(R) Core(TM) Ultra 7 165H GenuineIntel
- Binary: build/release/order_book_v3
- Execution: 100 consecutive process runs in the same shell session
- Workload: deterministic synthetic flow with mixed limit, market, and cancel requests

### Benchmark Results (Real Runs)

```bash
$ for((i=0; i<101; i++)); do ./order_book_v3 ; done | sort -n
Release :: 1000000 iters :: 35301500 iter/s :: 28.3 ns/iter :: 87.0 cyc/iter
Release :: 1000000 iters :: 36204276 iter/s :: 27.6 ns/iter :: 84.8 cyc/iter
Release :: 1000000 iters :: 36756199 iter/s :: 27.2 ns/iter :: 83.6 cyc/iter
Release :: 1000000 iters :: 37381638 iter/s :: 26.8 ns/iter :: 82.2 cyc/iter
Release :: 1000000 iters :: 37519465 iter/s :: 26.7 ns/iter :: 81.9 cyc/iter
Release :: 1000000 iters :: 37523480 iter/s :: 26.6 ns/iter :: 81.9 cyc/iter
Release :: 1000000 iters :: 37547187 iter/s :: 26.6 ns/iter :: 81.8 cyc/iter
Release :: 1000000 iters :: 37937061 iter/s :: 26.4 ns/iter :: 81.0 cyc/iter
Release :: 1000000 iters :: 38580775 iter/s :: 25.9 ns/iter :: 79.6 cyc/iter
Release :: 1000000 iters :: 38609494 iter/s :: 25.9 ns/iter :: 79.6 cyc/iter
...
Release :: 1000000 iters :: 40882589 iter/s :: 24.5 ns/iter :: 75.1 cyc/iter # median
...
Release :: 1000000 iters :: 42166042 iter/s :: 23.7 ns/iter :: 72.9 cyc/iter
Release :: 1000000 iters :: 42181513 iter/s :: 23.7 ns/iter :: 72.8 cyc/iter
Release :: 1000000 iters :: 42184840 iter/s :: 23.7 ns/iter :: 72.8 cyc/iter
Release :: 1000000 iters :: 42188387 iter/s :: 23.7 ns/iter :: 72.8 cyc/iter
Release :: 1000000 iters :: 42220002 iter/s :: 23.7 ns/iter :: 72.8 cyc/iter
Release :: 1000000 iters :: 42223551 iter/s :: 23.7 ns/iter :: 72.7 cyc/iter
Release :: 1000000 iters :: 42281302 iter/s :: 23.7 ns/iter :: 72.6 cyc/iter
Release :: 1000000 iters :: 42282899 iter/s :: 23.7 ns/iter :: 72.6 cyc/iter
Release :: 1000000 iters :: 42342441 iter/s :: 23.6 ns/iter :: 72.5 cyc/iter
Release :: 1000000 iters :: 42361280 iter/s :: 23.6 ns/iter :: 72.5 cyc/iter
```
  
## Matching Engine Benchmark integration

https://github.com/flash1-dev/matching-engine-benchmark/blob/main/README.md

Our matching engine is integrated with the open-source Matching Engine Algorithm Performance Challenge, a reproducible benchmarking framework designed to evaluate publicly available FIFO matching engines under identical conditions. The project provides a standardized workload, a common correctness oracle, and a unified integration model, allowing different matching engine implementations to be compared on a fair, like-for-like basis. The benchmark has been used to evaluate hundreds of open-source matching engines across multiple languages and architectures.

To participate in the benchmark, we developed a dedicated adapter that exposes our Order Book v3 implementation through the interface expected by the flash1-dev harness. The adapter acts as a lightweight compatibility layer, enabling the benchmark runner to load, execute, and validate our matching engine using the same methodology applied to every other engine in the benchmark ecosystem.

In addition, we created a build integration patch for the benchmark's baseline build pipeline. This allows our engine to be built and executed using the same workflow and conventions employed throughout the repository, making performance and correctness comparisons straightforward, reproducible, and consistent with the benchmark's established methodology.

Implemented components

• flash1-dev/adapters/lcv3_adapter.cpp
 Adapter exposing order_book v3 through the flash1-dev benchmark interface.

• flash1-dev/scripts/build_baselines.sh.patch
 Build pipeline extension enabling automated compilation and benchmarking alongside the repository's reference implementations.

 ```bash
$ ./harness --baseline lcv3 --scenario normal --mode perf
Loading engine: ./lcv3_adapter.so
Running benchmark (1996097 messages, scenario=normal, mode=perf)...
  Messages processed: 1996097
  Trades emitted:     62474
  Wall time:          0.1176 s
  Throughput:         16.97 M msgs/s
  Engine threads:     after_init 2, after_run 2 (+0) — informational
  Pre-build cost:     1.5 ns/msg (3% of the timed run) — informational

Correctness check:
  Expected hash: e9b2e3926854410b01a035aeb25dd05f1e597c233be8263de9eb49b6713d367e
  Computed hash: e9b2e3926854410b01a035aeb25dd05f1e597c233be8263de9eb49b6713d367e
  Status: PASS

Verdict: VALID

Result file: results/lcv3_normal_perf_s23_20261003T212048_p18608.json
```
