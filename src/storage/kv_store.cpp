#include "kv_store.h"
#include <iostream>

KVStore::KVStore(size_t hashCap, size_t lruCap)
    : store(hashCap), cache(lruCap)
{
    ttl.onExpiry([this](const std::string& key) { doExpire(key); });
    ttl.start();
    loadAOF();
}

KVStore::~KVStore() { ttl.stop(); }

// Called by TTL background thread (outside ttl.mu) or by cmdGet (lazy expiry).
void KVStore::doExpire(const std::string& key) {
    std::unique_lock<std::shared_mutex> lk(rwmu);
    if (store.del(key)) aof.logDel(key);  // only log if key existed
    cache.evict(key);
}

std::string KVStore::cmdSet(const std::string& key, const std::string& val) {
    // Remove any existing TTL BEFORE acquiring rwmu
    // (avoids: rwmu -> ttl.mu ordering conflict)
    ttl.removeExpiry(key);

    std::unique_lock<std::shared_mutex> lk(rwmu);
    store.set(key, val);
    cache.put(key, val);
    aof.logSet(key, val);
    return "+OK\r\n";
}

std::string KVStore::cmdGet(const std::string& key) {
    // Lazy expiry: check before read lock
    if (ttl.isExpired(key)) {
        doExpire(key);
        return "$-1\r\n";
    }

    std::shared_lock<std::shared_mutex> lk(rwmu);
    std::string val;
    if (cache.get(key, val))
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    if (store.get(key, val))
        return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
    return "$-1\r\n";
}

std::string KVStore::cmdDel(const std::string& key) {
    std::unique_lock<std::shared_mutex> lk(rwmu);
    ttl.removeExpiry(key);
    cache.evict(key);
    bool removed = store.del(key);
    if (removed) aof.logDel(key);
    return removed ? ":1\r\n" : ":0\r\n";
}

std::string KVStore::cmdExpire(const std::string& key, long long secs) {
    {   // check existence under shared lock
        std::shared_lock<std::shared_mutex> lk(rwmu);
        if (!store.exists(key)) return ":0\r\n";
    }
    // Set TTL outside rwmu to avoid lock-order issues
    ttl.setExpiry(key, secs * 1000);
    aof.logExpire(key, secs * 1000);
    return ":1\r\n";
}

std::string KVStore::cmdTTL(const std::string& key) {
    long long ms = ttl.getRemainingMs(key);
    if (ms < 0) return ":-1\r\n";
    return ":" + std::to_string(ms / 1000) + "\r\n";
}

void KVStore::loadAOF() {
    aof.replay(
        [this](const std::string& k, const std::string& v) {
            store.set(k, v); cache.put(k, v);
        },
        [this](const std::string& k) {
            store.del(k); cache.evict(k);
        },
        [this](const std::string& k, long long ttlMs) {
            ttl.setExpiry(k, ttlMs);
        }
    );
    std::cout << "[KVStore] AOF replay done.\n";
}