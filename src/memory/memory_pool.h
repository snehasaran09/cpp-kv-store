#pragma once
#include <cstddef>
#include <stdexcept>

// Arena allocator: pre-allocates a fixed slab, hands out aligned chunks in O(1).
// reset() reclaims everything at once — zero fragmentation.
class MemoryPool {
public:
    explicit MemoryPool(size_t size);
    ~MemoryPool();

    void*  allocate(size_t size);   // throws std::bad_alloc if exhausted
    void   reset();                  // free all allocations at once
    size_t used()     const { return offset; }
    size_t capacity() const { return poolSize; }

private:
    char*  pool;
    size_t poolSize;
    size_t offset;
};