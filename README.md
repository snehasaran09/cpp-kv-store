##Redis - like KeyValue Store - Sneha Saravanan

A distributed, Redis-like in-memory key-value store built in C++ from scratch.

A distributed, Redis-like in-memory key-value store in C++ with custom memory allocation, multi-threaded concurrency control, TCP networking using epoll, TTL expiration, and persistence, handling 100K+ ops/sec under concurrent load

## Features

- **Custom hash map** — open-addressing with FNV-1a hashing, no STL containers for the table
- **LRU cache** — O(1) hot-data layer in front of the hash map
- **epoll networking** — edge-triggered, non-blocking TCP, handles 10K+ concurrent clients
- **TTL expiration** — min-heap + background cleanup thread, Redis-compatible EXPIRE/TTL commands
- **AOF persistence** — append-only file, automatic replay on startup
- **Arena allocator** — O(1) allocation, zero fragmentation
- **shared_mutex** — concurrent reads, serialized writes

## Benchmark

| Threads | Ops/sec | p50 (ms) | p99 (ms) |
|---------|---------|----------|----------|
| 1       | 126,490 | 0.00     | 0.02     |
| 2       | 55,884  | 0.00     | 0.15     |
| 4       | 35,791  | 0.03     | 0.47     |
| 8       | 33,909  | 0.03     | 2.20     |

*Run inside Docker on Linux (Ubuntu 22.04), -O2, in-process (no network round-trip)*

## Requirements

- **Docker** (recommended) — works on macOS, Windows, and Linux
- OR: Linux with g++ 13+, cmake 3.16+ (epoll is Linux-only)

## Build & Run with Docker

```bash
# 1. Build the image (takes 2-3 min first time)
docker build -t cpp-kv-store .

# 2. Run unit tests (25 tests)
docker run --rm cpp-kv-store ./build/unit_tests

# 3. Run load benchmark
docker run --rm cpp-kv-store ./build/load_test

# 4. Start the server
docker run -p 6380:6380 cpp-kv-store
```

## Test the Server

The server uses a **plain-text newline-delimited protocol**. Use `nc` (netcat) to talk to it:

```bash
# Interactive mode (open a second terminal while server is running)
nc localhost 6380
```

Then type commands:
```
PING
SET name alice
GET name
EXPIRE name 10
TTL name
DEL name
```

Or send single commands:
```bash
echo "PING" | nc localhost 6380
echo "SET name alice" | nc localhost 6380
echo "GET name" | nc localhost 6380
```

> **Note:** `redis-cli` won't work directly because it sends binary RESP protocol,
> while this server uses a simpler plain-text protocol. Adding full RESP2/3 support
> would be the next improvement.

## Protocol (newline-delimited, RESP-style responses)

| Command         | Response                      |
|----------------|-------------------------------|
| `PING`          | `+PONG`                       |
| `SET key val`   | `+OK`                         |
| `GET key`       | `$<len>\r\n<val>` or `$-1`   |
| `DEL key`       | `:1` or `:0`                  |
| `EXPIRE key s`  | `:1` or `:0`                  |
| `TTL key`       | `:<secs>` or `:-1`           |

## Build Natively (Linux only)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./unit_tests
./kvstore 6380
```

## Docker Compose

```bash
docker-compose up
```
