
# LC++ Łukasz Czerwiński offers

## Data Structures & Memory Architecture
Design and implementation of custom data structures optimized for locality, cache behavior, and explicit ownership. LC++ delivers deterministic trees, allocators, containers, and memory layouts tailored to real-time workloads and performance-critical systems.

## Algorithms & Performance Engineering
Development and refinement of algorithms where latency, throughput, and predictability matter. LC++ focuses on branchless logic, cache-aware processing, and zero-overhead abstractions. I help clients diagnose bottlenecks, eliminate accidental complexity, and achieve stable performance under load.

## Matching Engine & Market Infrastructure
Architecture and implementation of matching engines, order-handling pipelines, and market-data components. LC++ provides deterministic event processing, predictable latency behavior, and clean, maintainable designs suitable for trading systems and other real-time environments.

## Consulting & Technical Advisory
Long-term or on-demand consulting for teams that need clarity, structure, and engineering discipline. LC++ supports modernization of legacy codebases, redesign of critical components, and strategic guidance for systems where correctness and performance are equally important.

## DEMO

[![Build](https://github.com/lc-lukaszczerwinski/public/actions/workflows/build.yml/badge.svg)](https://github.com/lc-lukaszczerwinski/public/actions/workflows/build.yml)

**matching_engine_v3** - a minimalistic matching engine built with zero external dependencies (no STL, no Boost, no runtime allocations). It implements limit orders, market orders, immediate-or-cancel (IOC), insert, quantity update and cancellation, producing events in just a few nanoseconds. Internally this is a treap (price priority) of intrusive lists (FIFO priority).

### Benchmark Methodology

- Platform: Intel(R) Core(TM) Ultra 7 165H GenuineIntel
- Binary: build/release/matching_engine_v3
- Mode: Release
- Execution: 10 consecutive process runs in the same shell session
- Command used: for((i=0;i<10;i++)); do ./matching_engine_v3 ; done
- Workload: deterministic synthetic flow with mixed limit, market, and cancel requests
- Reported metrics:
- Latency (ns/iter): p50 and p99
- Throughput (iter/s): two quantiles printed by the binary
- CPU (cycles/iter): p50 and p99

### Benchmark Results (Real Runs)

```bash
Release :: iters=119122 :: p50
Latency (ns/iter): p50=20.3 :: CPU (cycles/iter): p50=62.5 :: Throughput (iter/s): p50=49193008
Latency (ns/iter): p50=20.5 :: CPU (cycles/iter): p50=63.1 :: Throughput (iter/s): p50=48672599
Latency (ns/iter): p50=20.6 :: CPU (cycles/iter): p50=63.4 :: Throughput (iter/s): p50=48478855
Latency (ns/iter): p50=20.6 :: CPU (cycles/iter): p50=63.3 :: Throughput (iter/s): p50=48551508
Latency (ns/iter): p50=20.7 :: CPU (cycles/iter): p50=63.5 :: Throughput (iter/s): p50=48339926
Latency (ns/iter): p50=20.8 :: CPU (cycles/iter): p50=63.9 :: Throughput (iter/s): p50=48114491
Latency (ns/iter): p50=20.8 :: CPU (cycles/iter): p50=63.8 :: Throughput (iter/s): p50=48161625
Latency (ns/iter): p50=20.8 :: CPU (cycles/iter): p50=64.0 :: Throughput (iter/s): p50=47973504
Latency (ns/iter): p50=21.0 :: CPU (cycles/iter): p50=64.4 :: Throughput (iter/s): p50=47699323
Latency (ns/iter): p50=20.9 :: CPU (cycles/iter): p50=64.2 :: Throughput (iter/s): p50=47838201

Release :: iters=119122 :: p99
Latency (ns/iter): p99=21.2 :: CPU (cycles/iter): p99=65.0 :: Throughput (iter/s): p1=47426540
Latency (ns/iter): p99=21.6 :: CPU (cycles/iter): p99=66.5 :: Throughput (iter/s): p1=46315164
Latency (ns/iter): p99=22.0 :: CPU (cycles/iter): p99=67.6 :: Throughput (iter/s): p1=46215043
Latency (ns/iter): p99=21.6 :: CPU (cycles/iter): p99=66.3 :: Throughput (iter/s): p1=47430506
Latency (ns/iter): p99=22.2 :: CPU (cycles/iter): p99=68.2 :: Throughput (iter/s): p1=46154609
Latency (ns/iter): p99=22.2 :: CPU (cycles/iter): p99=68.1 :: Throughput (iter/s): p1=46353985
Latency (ns/iter): p99=22.1 :: CPU (cycles/iter): p99=67.7 :: Throughput (iter/s): p1=45359540
Latency (ns/iter): p99=21.9 :: CPU (cycles/iter): p99=67.2 :: Throughput (iter/s): p1=46217051
Latency (ns/iter): p99=22.3 :: CPU (cycles/iter): p99=68.5 :: Throughput (iter/s): p1=46211421
Latency (ns/iter): p99=22.0 :: CPU (cycles/iter): p99=67.5 :: Throughput (iter/s): p1=45912694
```
  