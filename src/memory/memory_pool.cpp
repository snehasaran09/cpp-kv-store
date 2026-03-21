#include "memory_pool.h"
#include <cstdlib>

MemoryPool::MemoryPool(size_t size) : poolSize(size), offset(0) {
    pool = static_cast<char*>(malloc(size));
    if (!pool) throw std::bad_alloc();
}

MemoryPool::~MemoryPool() { free(pool); }

void* MemoryPool::allocate(size_t size) {
    // Round up to 8-byte alignment for cache friendliness
    size_t aligned = (size + 7) & ~static_cast<size_t>(7);
    if (offset + aligned > poolSize) throw std::bad_alloc();
    void* ptr = pool + offset;
    offset += aligned;
    return ptr;
}

void MemoryPool::reset() { offset = 0; }