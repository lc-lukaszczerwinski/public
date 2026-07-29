/*
 * Copyright (c) 2026 LC++ Lukasz Czerwinski
 */

#include <algorithm>
#include <random>
#include <set>

#include "matching_engine_v3.hpp"
#include "timer.hpp"

/*
 * Request Generator for randomized test with Laplace price distribution
 */

class RequestGenerator {
public: /* ctor, dtor */

  RequestGenerator() = default;

public: /* api */

  std::vector<v3::Request> generate(
      int32_t startPrice,
      int32_t endPrice,
      int32_t iters,
      int32_t laplaceScale,
      double probMarket,
      double probCancel) //
  {
    std::set<int32_t> activeIds;
    std::vector<v3::Request> out(iters);
    std::unordered_map<int32_t, int32_t> idPrice;
    std::unordered_map<int32_t, int32_t> idSide;

    idPrice.reserve(std::max(0, iters));
    idSide.reserve(std::max(0, iters));

    int32_t nextId = 1;
    const int32_t lo = std::min(startPrice, endPrice);
    const int32_t hi = std::max(startPrice, endPrice);
    const double step = (iters > 0) ? (1.0 * (endPrice - startPrice) / iters) : 0.0;
    const double scale = std::max(0, laplaceScale);

    for(int32_t i = 0; i < iters; i++) {
      auto& e = out[i];
      double u = _uni01(_rng);

      if((u < probCancel) && (activeIds.empty() == false)) {
        const int32_t probeId = 1 + int32_t(_uni01(_rng) * (nextId - 1));
        auto it = activeIds.lower_bound(probeId);

        if(it == activeIds.end()) {
          it = activeIds.begin();
        }

        e.id = *it;
        e.price = idPrice[e.id] * idSide[e.id];
        e.qty = 0;

        activeIds.erase(it);
        idPrice.erase(e.id);
        idSide.erase(e.id);
        continue;
      }

      e.id = nextId++;

      if(u < probCancel + probMarket) {
        e.price = 0;
        e.qty = 1 + int32_t(_uni01(_rng) * 10);

        if(_uni01(_rng) > 0.5) {
          e.qty = -e.qty;
        }

        continue;
      }

      e.qty = 1 + int32_t(_uni01(_rng) * 10);

      if(_uni01(_rng) > 0.5) {
        e.qty = -e.qty;
      }

      const double trendPrice = startPrice + (step * i);
      double laplaceNoise = 0.0;

      if(scale > 0.0) {
        const double s = _uniSigned(_rng);
        laplaceNoise = -scale * ((s < 0.0) ? -1.0 : 1.0) * std::log(1.0 - 2.0 * std::abs(s));
      }

      const int32_t candidate = int32_t(std::round(trendPrice + laplaceNoise));
      e.price = std::max(lo, std::min(candidate, hi));

      idPrice[e.id] = e.price;
      idSide[e.id] = (e.qty > 0) ? 1 : -1;
      activeIds.insert(e.id);
    }

    return out;
  }

private: /* members */

  std::mt19937 _rng{12345};
  std::uniform_real_distribution<double> _uni01{0.0, 1.0};
  std::uniform_real_distribution<double> _uniSigned{-0.5, 0.5};
};

struct BenchSample {
  int32_t events;
  double latencyNs;
  double throughput;
  double cpuCycles;
};

template<typename T>
T percentile(std::vector<T> values, double p) {
  if(values.empty()) {
    return T{};
  }

  std::sort(values.begin(), values.end());

  const double pos = std::max(0.0, std::min(1.0, p)) * (values.size() - 1);
  const size_t idx = std::min<size_t>(size_t(std::ceil(pos)), values.size() - 1);

  return values[idx];
}

template<typename Engine>
BenchSample benchmark_once(int32_t iters) {
  v3::QueueOut out;
  int32_t events = 0;
  Engine engine(out);

  struct Benchmark {
    Benchmark(Engine& engine, int32_t iters, int32_t& events)
      : _iters(iters)
      , _engine(engine)
      , _events(events) //
    {
    }

    int32_t _iters;
    int32_t& _events;
    Engine& _engine;
    std::vector<v3::Request> _requests;
    std::vector<int> _slots;

    void setup() {
      RequestGenerator gen;

      _slots.resize(_iters + 10);
      _slots.assign(_iters + 10, 0);
      _requests = gen.generate(
          /* startPrice  */ 30,
          /* endPrice    */ 170,
          /* iters       */ _iters,
          /* laplaceScale*/ 5,
          /* probMarket  */ 0.05,
          /* probCancel  */ 0.25);
    }

    void run() {
      _events = 0;

      for(const auto& e : _requests) {
        if(e.qty > 0) {
          if(e.price == 0) {
            _engine.template insert_mkt_order_ioc<Buy>(e.id, e.qty);
          } else {
            _slots[e.id] = _engine.template insert_order<Buy>(e.id, e.price, e.qty);
          }
        } else if(e.qty < 0) {
          if(e.price == 0) {
            _engine.template insert_mkt_order_ioc<Sell>(e.id, -e.qty);
          } else {
            _slots[e.id] = _engine.template insert_order<Sell>(e.id, e.price, -e.qty);
          }
        } else {
          if((e.price > 0) && (_slots[e.id] != -1)) {
            _engine.template cancel_order<Buy>(e.id, e.price, _slots[e.id]);
          } else if((e.price < 0) && (_slots[e.id] != -1)) {
            _engine.template cancel_order<Sell>(e.id, -e.price, _slots[e.id]);
          }
        }

#ifndef NDEBUG
        _events += _engine.out().log(to_string(e) + " => ");
#else
        _events += _engine.out().clear();
#endif
      }
    }

    void teardown() {
    }

  } bench(engine, iters, events);

  const Duration m = Timer<1>(bench);

  return {
      .events = events,
      .latencyNs = (events > 0) ? (1.0 * m.ns() / events) : 0.0,
      .throughput = (m.ns() > 0) ? (1e9 * events / m.ns()) : 0.0,
      .cpuCycles = (events > 0) ? (1.0 * m.cycles() / events) : 0.0,
  };
}

template<typename Engine>
void benchmark(int32_t iters, int32_t samples) {
  int32_t events = 0;

  std::vector<double> cpuSamples;
  cpuSamples.reserve(std::max(0, samples));

  std::vector<double> latencySamples;
  latencySamples.reserve(std::max(0, samples));

  std::vector<double> throughputSamples;
  throughputSamples.reserve(std::max(0, samples));

  for(int32_t i = 0; i < samples; ++i) {
    const BenchSample s = benchmark_once<Engine>(iters);
    events = s.events;

    cpuSamples.push_back(s.cpuCycles);
    latencySamples.push_back(s.latencyNs);
    throughputSamples.push_back(s.throughput);
  }

#ifdef HFT_DEBUG
  constexpr auto format = "{} :: "
                          "iters={}";

  std::cout << std::format(format,
                           PROFILE,
                           events) << std::endl;
#else
  constexpr auto format = "{} :: "
                          "iters={} :: "
                          "Latency (ns/iter): p50={:.1f} p99={:.1f} :: "
                          "CPU (cycles/iter): p50={:.1f} p99={:.1f} :: "
                          "Throughput (iter/s): p1={:.0f} p50={:.0f}";

  std::cout << std::format(format,
                           PROFILE,
                           events,
                           percentile(latencySamples, 0.50),
                           percentile(latencySamples, 0.99),
                           percentile(cpuSamples, 0.50),
                           percentile(cpuSamples, 0.99),
                           percentile(throughputSamples, 0.01), // the more the better
                           percentile(throughputSamples, 0.50)) << std::endl;
#endif
}

int main(int argc, char* argv[]) {

#ifdef HFT_DEBUG
  benchmark<v3::MatchingEngine>(100'000, 1);
#else
  benchmark<v3::MatchingEngine>(100'000, 100);
#endif

  return 0;
}
