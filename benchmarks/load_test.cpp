#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <string>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include "storage/kv_store.h"

static std::atomic<long long> gOps{0};

struct Stats {
    long long throughput;
    double p50, p99;
};

Stats runBenchmark(KVStore& kv, int nThreads, int opsPerThread) {
    std::vector<std::thread> threads;
    std::vector<std::vector<long long>> latencies(nThreads);
    gOps = 0;

    auto work = [&](int tid) {
        auto& lats = latencies[tid];
        lats.reserve(opsPerThread);
        for (int i = 0; i < opsPerThread; ++i) {
            std::string key = "k" + std::to_string(tid * opsPerThread + i);
            auto t0 = std::chrono::steady_clock::now();
            if (i % 4 == 0) {
                kv.cmdSet(key, "value");
            } else {
                kv.cmdGet(key);
            }
            auto t1 = std::chrono::steady_clock::now();
            lats.push_back(
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()
            );
            ++gOps;
        }
    };

    auto start = std::chrono::steady_clock::now();
    for (int t = 0; t < nThreads; ++t)
        threads.emplace_back(work, t);
    for (auto& th : threads) th.join();
    auto end = std::chrono::steady_clock::now();

    double secs = std::chrono::duration<double>(end - start).count();
    long long throughput = static_cast<long long>(gOps / secs);

    // Flatten latencies
    std::vector<long long> all;
    all.reserve(nThreads * opsPerThread);
    for (auto& v : latencies)
        all.insert(all.end(), v.begin(), v.end());
    std::sort(all.begin(), all.end());

    double p50 = all[all.size() * 50 / 100] / 1000.0;
    double p99 = all[all.size() * 99 / 100] / 1000.0;

    return {throughput, p50, p99};
}

int main() {
    std::cout << "=== Load Benchmark ===\n\n";
    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(14) << "Ops/sec"
              << std::setw(14) << "p50 (ms)"
              << std::setw(14) << "p99 (ms)"
              << "\n"
              << std::string(52, '-') << "\n";

    KVStore kv;
    for (int threads : {1, 2, 4, 8}) {
        int opsPerThread = 25000;
        auto s = runBenchmark(kv, threads, opsPerThread);
        std::cout << std::left
                  << std::setw(10) << threads
                  << std::setw(14) << s.throughput
                  << std::setw(14) << std::fixed << std::setprecision(2) << s.p50
                  << std::setw(14) << s.p99
                  << "\n";
    }
    std::cout << "\nNote: run on Linux with -O2 for production numbers.\n";
    return 0;
}
