#pragma once
#include <string>
#include <list>
#include <unordered_map>

// LRU cache: hot-path layer in front of the hash map.
// O(1) get/put/evict via doubly-linked list + unordered_map.
class LRUCache {
public:
    explicit LRUCache(size_t cap = 256);

    bool get(const std::string& key, std::string& out);
    void put(const std::string& key, const std::string& val);
    void evict(const std::string& key);
    bool contains(const std::string& key) const;
    size_t size() const { return map.size(); }

private:
    size_t cap;
    using KV = std::pair<std::string, std::string>;
    std::list<KV> lst;
    std::unordered_map<std::string, std::list<KV>::iterator> map;
};