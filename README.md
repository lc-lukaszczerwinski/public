
# LC++ | C++ and Python Consulting for Financial Systems

## Dependencies
### Ubuntu
sudo apt install -y libasio-dev # for asio client/server

## DEMO

[![Build](https://github.com/lc-lukaszczerwinski/public/actions/workflows/build.yml/badge.svg)](https://github.com/lc-lukaszczerwinski/public/actions/workflows/build.yml)

All order books are minimalistic, header-only implementations with zero external dependencies. They support limit orders, market orders, immediate-or-cancel (IOC) orders, order updates, and cancellations.

| Version | Levels | Orders | OrderId resolution | time (p50) | throughput (p50) |
|---|---|---|---|---|---|
| v2 | Fixed (array) | Fixed (static pool) | Resolves via SlotId | 21.0 ns | 47,610,441 |
| v3 | Unlimited (treap) | Fixed (static pool) | Resolves via SlotId | 23.7 ns | 42,243,974 |
| v4 | Unlimited (treap) | Unlimited (dynamic pool) | Internally (open-hash) | 28.5 ns | 35,035,986 |

### Benchmark Methodology

- Platform: Intel(R) Core(TM) Ultra 7 165H GenuineIntel
- Binary: build/release/order_book_v3
- Execution: 100 consecutive process runs in the same shell session
- Workload: deterministic synthetic flow with mixed limit, market, and cancel requests

### Detailed Benchmark Results

```bash
$ for((i=0;i<101;i++)); do ./order_book_v2 ; done | sort -n
Release :: 1000000 iters :: 41295700 iter/s :: 24.2 ns/iter :: 74.4 cyc/iter
Release :: 1000000 iters :: 41480335 iter/s :: 24.1 ns/iter :: 74.0 cyc/iter
Release :: 1000000 iters :: 41556923 iter/s :: 24.1 ns/iter :: 73.9 cyc/iter
Release :: 1000000 iters :: 41570332 iter/s :: 24.1 ns/iter :: 73.9 cyc/iter
Release :: 1000000 iters :: 44772070 iter/s :: 22.3 ns/iter :: 68.6 cyc/iter
...
Release :: 1000000 iters :: 47610441 iter/s :: 21.0 ns/iter :: 64.5 cyc/iter # median
...
Release :: 1000000 iters :: 47641221 iter/s :: 21.0 ns/iter :: 64.5 cyc/iter
Release :: 1000000 iters :: 47655290 iter/s :: 21.0 ns/iter :: 64.5 cyc/iter
Release :: 1000000 iters :: 47784557 iter/s :: 20.9 ns/iter :: 64.3 cyc/iter
Release :: 1000000 iters :: 47911740 iter/s :: 20.9 ns/iter :: 64.1 cyc/iter
Release :: 1000000 iters :: 47926932 iter/s :: 20.9 ns/iter :: 64.1 cyc/iter
```

```bash
$ for((i=0;i<101;i++)); do ./order_book_v3 ; done | sort -n
Release :: 1000000 iters :: 36186591 iter/s :: 27.6 ns/iter :: 84.9 cyc/iter
Release :: 1000000 iters :: 36355976 iter/s :: 27.5 ns/iter :: 84.5 cyc/iter
Release :: 1000000 iters :: 36740787 iter/s :: 27.2 ns/iter :: 83.6 cyc/iter
Release :: 1000000 iters :: 36989531 iter/s :: 27.0 ns/iter :: 83.0 cyc/iter
Release :: 1000000 iters :: 39386725 iter/s :: 25.4 ns/iter :: 78.0 cyc/iter
...
Release :: 1000000 iters :: 42243974 iter/s :: 23.7 ns/iter :: 72.7 cyc/iter # median
...
Release :: 1000000 iters :: 42268937 iter/s :: 23.7 ns/iter :: 72.7 cyc/iter
Release :: 1000000 iters :: 42281706 iter/s :: 23.7 ns/iter :: 72.6 cyc/iter
Release :: 1000000 iters :: 42299924 iter/s :: 23.6 ns/iter :: 72.6 cyc/iter
Release :: 1000000 iters :: 42355738 iter/s :: 23.6 ns/iter :: 72.5 cyc/iter
Release :: 1000000 iters :: 42363527 iter/s :: 23.6 ns/iter :: 72.5 cyc/iter
```
  
```bash
$ for((i=0;i<101;i++)); do ./order_book_v4 ; done | sort -n
Release :: 1000000 iters :: 28152300 iter/s :: 35.5 ns/iter :: 109.1 cyc/iter
Release :: 1000000 iters :: 28504660 iter/s :: 35.1 ns/iter :: 107.8 cyc/iter
Release :: 1000000 iters :: 29220092 iter/s :: 34.2 ns/iter :: 105.1 cyc/iter
Release :: 1000000 iters :: 29283969 iter/s :: 34.1 ns/iter :: 104.9 cyc/iter
Release :: 1000000 iters :: 32671074 iter/s :: 30.6 ns/iter :: 94.0 cyc/iter
...
Release :: 1000000 iters :: 35035986 iter/s :: 28.5 ns/iter :: 87.7 cyc/iter # median
...
Release :: 1000000 iters :: 35175620 iter/s :: 28.4 ns/iter :: 87.3 cyc/iter
Release :: 1000000 iters :: 35193717 iter/s :: 28.4 ns/iter :: 87.3 cyc/iter
Release :: 1000000 iters :: 35275694 iter/s :: 28.3 ns/iter :: 87.1 cyc/iter
Release :: 1000000 iters :: 35388728 iter/s :: 28.3 ns/iter :: 86.8 cyc/iter
Release :: 1000000 iters :: 35519978 iter/s :: 28.2 ns/iter :: 86.5 cyc/iter
```

## asio client + asio server with a minimal FIX session

![demo](./docs/images/demo.svg)

## 🏆Matching Engine Benchmark Award

 Order Book v3 has been listed as the fastest open-source conforming implementation evaluated on the benchmark harness (October 2026) by the flash1-dev [Matching Engine Benchmark](https://github.com/flash1-dev/matching-engine-benchmark).

| Engine | Language | Conformance | Worst-case M/s | Published figure | Notes |
|:-------|:---------|:------------|:---------------|:-----------------|:------|
| lcv3 / LC++ v3 | C++ | as shipped | 9.25 (normal) | ~42M/s | treap price levels + intrusive FIFO; ARM build needs an empty stand-in for the engine's x86-only `immintrin.h` include (engine unmodified) |
