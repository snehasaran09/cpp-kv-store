#include "ttl/ttl_manager.h"

TTLManager::TTLManager()  = default;
TTLManager::~TTLManager() { stop(); }

TTLManager::TimeMs TTLManager::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TTLManager::setExpiry(const std::string& key, long long ttlMs) {
    std::lock_guard<std::mutex> lk(mu);
    TimeMs exp = nowMs() + ttlMs;
    expiry[key] = exp;
    heap.push({exp, key});
}

void TTLManager::removeExpiry(const std::string& key) {
    std::lock_guard<std::mutex> lk(mu);
    expiry.erase(key);  // heap entry becomes stale; cleaned up lazily
}

bool TTLManager::isExpired(const std::string& key) const {
    std::lock_guard<std::mutex> lk(mu);
    auto it = expiry.find(key);
    if (it == expiry.end()) return false;
    return nowMs() >= it->second;
}

long long TTLManager::getRemainingMs(const std::string& key) const {
    std::lock_guard<std::mutex> lk(mu);
    auto it = expiry.find(key);
    if (it == expiry.end()) return -1;
    long long rem = it->second - nowMs();
    return rem > 0 ? rem : 0;
}

void TTLManager::start() {
    running = true;
    bgThread = std::thread(&TTLManager::cleanupLoop, this);
}

void TTLManager::stop() {
    running = false;
    if (bgThread.joinable()) bgThread.join();
}

void TTLManager::cleanupLoop() {
    while (running) {
        std::vector<std::string> expired;

        {   // Narrow lock scope: collect expired keys, then release before calling back
            std::lock_guard<std::mutex> lk(mu);
            TimeMs now = nowMs();
            while (!heap.empty()) {
                // Structured binding (C++17): copy top values before pop()
                auto [expAt, key] = heap.top();
                auto it = expiry.find(key);
                // Lazy-invalidate stale entries (key deleted or TTL updated)
                if (it == expiry.end() || it->second != expAt) {
                    heap.pop(); continue;
                }
                if (now >= expAt) {
                    expired.push_back(key);
                    expiry.erase(key);
                    heap.pop();
                } else {
                    break;  // heap is sorted; nothing earlier will expire
                }
            }
        }   // mu released HERE — callbacks run lock-free

        for (const auto& k : expired)
            if (callback) callback(k);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}