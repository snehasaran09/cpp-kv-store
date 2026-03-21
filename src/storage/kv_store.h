#pragma once
#include <string>
#include <shared_mutex>
#include "hash_map.h"
#include "lru_cache.h"
#include "ttl/ttl_manager.h"
#include "persistence/aof_log.h"

// Central KV Store: ties together all subsystems.
// Read-heavy workload: shared_mutex allows concurrent GET operations.
// Write operations (SET/DEL) hold exclusive lock.
class KVStore {
public:
    KVStore(size_t hashCap = 65536, size_t lruCap = 1024);
    ~KVStore();

    std::string cmdSet   (const std::string& key, const std::string& val);
    std::string cmdGet   (const std::string& key);
    std::string cmdDel   (const std::string& key);
    std::string cmdExpire(const std::string& key, long long secs);
    std::string cmdTTL   (const std::string& key);
    std::string cmdPing  () { return "+PONG\r\n"; }

private:
    HashMap    store;   // primary storage (custom hash map)
    LRUCache   cache;   // hot-data cache
    TTLManager ttl;     // expiry manager
    AOFLog     aof;     // persistence

    mutable std::shared_mutex rwmu;   // shared = reads, unique = writes

    void doExpire(const std::string& key);
    void loadAOF();
};