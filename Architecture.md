# Architecture

## ASCII Diagram

```
┌──────────────────────────────────────────────────────────┐
│                      TCP Clients                          │
│           (nc / netcat or any TCP client)                 │
└─────────────────────────┬────────────────────────────────┘
                          │ TCP (port 6380)
                          │ Plain-text, newline-delimited
                          ▼
┌──────────────────────────────────────────────────────────┐
│              Docker Container (Ubuntu 22.04)              │
│                                                          │
│  ┌────────────────────────────────────────────────────┐  │
│  │               TCPServer (epoll)                    │  │
│  │  Non-blocking sockets  · Edge-triggered (EPOLLET)  │  │
│  │  Per-client recv buffers · Newline protocol        │  │
│  └──────────────────────┬─────────────────────────────┘  │
│                         │ dispatch(cmd)                   │
│                         ▼                                 │
│  ┌────────────────────────────────────────────────────┐  │
│  │                    KVStore                         │  │
│  │      shared_mutex: shared=reads, exclusive=writes  │  │
│  │                                                    │  │
│  │  ┌──────────┐  ┌──────────┐  ┌────────────────┐   │  │
│  │  │ HashMap  │  │ LRUCache │  │  TTLManager    │   │  │
│  │  │ FNV-1a   │  │ list+map │  │ min-heap+thread│   │  │
│  │  │ open-addr│  │ O(1)     │  │ fires@50ms     │   │  │
│  │  └──────────┘  └──────────┘  └────────────────┘   │  │
│  │                                                    │  │
│  │  ┌──────────┐  ┌──────────────────────────────┐   │  │
│  │  │MemoryPool│  │          AOFLog               │   │  │
│  │  │ arena    │  │ append-only file, replay      │   │  │
│  │  └──────────┘  └──────────────────────────────┘   │  │
│  └────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────┘
```

## Running Environment

This project uses **epoll**, a Linux-only kernel feature. It is built and run inside a Docker container (Ubuntu 22.04) so it works on any host OS — macOS, Windows, or Linux.

```
Host (macOS/Windows/Linux)
  └── Docker Desktop
        └── Ubuntu 22.04 container
              └── kvstore binary (compiled with g++ -O2)
```

## Component Details

### 1. Networking — `src/server/`

- **EpollLoop**: thin wrapper around Linux `epoll_create1` / `epoll_wait`
  - Edge-triggered (`EPOLLET`): socket must be drained until `EAGAIN`
  - 100ms `epoll_wait` timeout for clean shutdown
- **TCPServer**: accepts connections, routes commands to KVStore
  - One event-loop thread handles all clients (no thread-per-connection)
  - Per-client `std::string` buffer accumulates partial commands
- **Protocol**: plain-text, newline-delimited
  - Compatible with `nc` (netcat)
  - Not compatible with `redis-cli` (which uses binary RESP protocol)

### 2. Storage Engine — `src/storage/`

- **HashMap**: custom open-addressing table
  - FNV-1a 64-bit hash: deterministic, uniform distribution
  - Tombstone deletion: deleted slots remain probing-visible
  - Auto-rehash at 50% load factor (doubles capacity)
- **LRUCache**: doubly-linked list + `unordered_map` for O(1) get/put/evict
  - Evicts LRU entry on capacity overflow
  - Sits in front of HashMap: GET checks cache first

### 3. Concurrency — `src/storage/kv_store.cpp`

- `std::shared_mutex rwmu`:
  - Multiple readers acquire shared lock simultaneously
  - Writers acquire exclusive lock, block all readers
- Lock ordering (prevents deadlock):
  - `rwmu` is always acquired before `ttl.mu` (if both needed)
  - TTL background thread releases `ttl.mu` **before** invoking callbacks that need `rwmu`

### 4. Memory — `src/memory/`

- Arena allocator: `malloc` one large slab at startup
- `allocate(n)`: bump `offset` by `align(n)`, O(1)
- `reset()`: resets offset to 0, bulk-reclaims all memory
- **Note**: currently used standalone; not yet wired into HashMap's hot path

### 5. Persistence — `src/persistence/`

- AOF (Append-Only File): every `SET`, `DEL`, `EXPIRE` is written + flushed
- `replay()` on startup: re-executes all logged commands to restore state
- Format: `SET key value\n`, `DEL key\n`, `EXPIRE key ms\n`

### 6. TTL — `src/ttl/`

- Min-heap `priority_queue<(expiry_ms, key), ..., std::greater>`: smallest expiry at top
- `expiryMap`: authoritative `key → expiry_ms` (heap can have stale entries)
- Background thread wakes every 50ms:
  1. Lock `ttl.mu`, pop expired entries, collect keys, unlock
  2. Invoke callbacks **outside** lock → no deadlock with `rwmu`

## Tradeoffs

| Decision | Why |
|----------|-----|
| `shared_mutex` | Optimized for read-heavy workloads; concurrent GETs don't block each other |
| Open addressing | Better cache locality than chaining; fewer allocations |
| AOF over snapshots | Simpler recovery; no fork required; trades disk space for simplicity |
| Background TTL thread | Predictable expiry latency; doesn't add per-request overhead |
| Callbacks outside `ttl.mu` | Prevents lock-order deadlock between `ttl.mu` and `rwmu` |
| Single epoll thread | Eliminates lock contention on the I/O path; simpler mental model |
| Plain-text protocol | Simpler to implement and debug; tradeoff is no redis-cli compatibility |
| Docker deployment | epoll is Linux-only; Docker lets it run on any host OS |

## Known Limitations

- No AOF compaction (file grows unbounded)
- No replication or clustering
- MemoryPool not yet on the HashMap hot path
- Plain-text protocol (not binary RESP2/3) — incompatible with redis-cli
- No AUTH, no keyspace notifications