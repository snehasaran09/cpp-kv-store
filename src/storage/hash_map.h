#pragma once
#include <string>
#include <cstddef>

// Custom open-addressing hash map with FNV-1a hashing + tombstone deletion.
// No STL containers used for the underlying table.
struct HashEntry {
    std::string key;
    std::string value;
    bool occupied  = false;
    bool tombstone = false;
};

class HashMap {
public:
    explicit HashMap(size_t capacity = 1024);
    ~HashMap();

    bool   set(const std::string& key, const std::string& value);
    bool   get(const std::string& key, std::string& out) const;
    bool   del(const std::string& key);
    bool   exists(const std::string& key) const;
    size_t size() const { return count; }

private:
    HashEntry* table;
    size_t     cap;
    size_t     count;

    size_t fnvHash(const std::string& key) const;
    void   rehash(size_t newCap);
};
