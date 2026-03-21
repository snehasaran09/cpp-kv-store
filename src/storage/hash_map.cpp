#include "hash_map.h"

HashMap::HashMap(size_t capacity) : cap(capacity), count(0) {
    table = new HashEntry[cap];
}

HashMap::~HashMap() { delete[] table; }

// FNV-1a 64-bit hash — deterministic, good distribution
size_t HashMap::fnvHash(const std::string& key) const {
    size_t h = 14695981039346656037ULL;
    for (unsigned char c : key) { h ^= c; h *= 1099511628211ULL; }
    return h % cap;
}

bool HashMap::set(const std::string& key, const std::string& value) {
    if (count * 2 >= cap) rehash(cap * 2);   // keep load < 50%

    size_t idx = fnvHash(key);
    size_t firstTomb = size_t(-1);

    for (size_t i = 0; i < cap; ++i) {
        size_t pos = (idx + i) % cap;

        if (!table[pos].occupied && !table[pos].tombstone) {
            size_t ins = (firstTomb != size_t(-1)) ? firstTomb : pos;
            table[ins] = {key, value, true, false};
            ++count;
            return true;
        }
        if (table[pos].tombstone) {
            if (firstTomb == size_t(-1)) firstTomb = pos;
            continue;
        }
        if (table[pos].key == key) {
            table[pos].value = value;   // overwrite existing key
            return true;
        }
    }
    if (firstTomb != size_t(-1)) {
        table[firstTomb] = {key, value, true, false};
        ++count;
        return true;
    }
    return false;
}

bool HashMap::get(const std::string& key, std::string& out) const {
    size_t idx = fnvHash(key);
    for (size_t i = 0; i < cap; ++i) {
        size_t pos = (idx + i) % cap;
        if (!table[pos].occupied && !table[pos].tombstone) return false;
        if (table[pos].tombstone) continue;
        if (table[pos].key == key) { out = table[pos].value; return true; }
    }
    return false;
}

bool HashMap::del(const std::string& key) {
    size_t idx = fnvHash(key);
    for (size_t i = 0; i < cap; ++i) {
        size_t pos = (idx + i) % cap;
        if (!table[pos].occupied && !table[pos].tombstone) return false;
        if (table[pos].tombstone) continue;
        if (table[pos].key == key) {
            table[pos].occupied  = false;
            table[pos].tombstone = true;
            --count;
            return true;
        }
    }
    return false;
}

bool HashMap::exists(const std::string& key) const {
    std::string tmp;
    return get(key, tmp);
}

void HashMap::rehash(size_t newCap) {
    HashEntry* old    = table;
    size_t     oldCap = cap;
    cap   = newCap;
    count = 0;
    table = new HashEntry[cap];
    for (size_t i = 0; i < oldCap; ++i)
        if (old[i].occupied) set(old[i].key, old[i].value);
    delete[] old;
}