#include "lru_cache.h"

LRUCache::LRUCache(size_t cap) : cap(cap) {}

bool LRUCache::get(const std::string& key, std::string& out) {
    auto it = map.find(key);
    if (it == map.end()) return false;
    lst.splice(lst.begin(), lst, it->second);   // move to front
    out = it->second->second;
    return true;
}

void LRUCache::put(const std::string& key, const std::string& val) {
    auto it = map.find(key);
    if (it != map.end()) {
        it->second->second = val;
        lst.splice(lst.begin(), lst, it->second);
        return;
    }
    if (lst.size() >= cap) {
        map.erase(lst.back().first);    // evict least-recently-used
        lst.pop_back();
    }
    lst.push_front({key, val});
    map[key] = lst.begin();
}

void LRUCache::evict(const std::string& key) {
    auto it = map.find(key);
    if (it == map.end()) return;
    lst.erase(it->second);
    map.erase(it);
}

bool LRUCache::contains(const std::string& key) const {
    return map.count(key) > 0;
}