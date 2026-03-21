#pragma once
#include <string>
#include <queue>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <utility>

// TTL manager: min-heap tracks (expiry_ms, key) pairs.
// Background thread wakes every 50ms, pops expired keys, fires callbacks.
// Callbacks are invoked OUTSIDE the internal lock to prevent deadlocks.
class TTLManager {
public:
    TTLManager();
    ~TTLManager();

    void setExpiry(const std::string& key, long long ttlMs);
    void removeExpiry(const std::string& key);
    bool isExpired(const std::string& key) const;
    long long getRemainingMs(const std::string& key) const;  // -1 = no TTL

    void onExpiry(std::function<void(const std::string&)> cb) { callback = std::move(cb); }
    void start();
    void stop();

private:
    using TimeMs   = long long;
    using HeapItem = std::pair<TimeMs, std::string>;

    // Min-heap: smallest expiry at top
    std::priority_queue<HeapItem,
                        std::vector<HeapItem>,
                        std::greater<HeapItem>> heap;
    std::unordered_map<std::string, TimeMs> expiry;  // authoritative map

    mutable std::mutex mu;
    std::thread bgThread;
    std::atomic<bool> running{false};
    std::function<void(const std::string&)> callback;

    static TimeMs nowMs();
    void cleanupLoop();
};