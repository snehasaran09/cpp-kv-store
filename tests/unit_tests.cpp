#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "storage/hash_map.h"
#include "storage/lru_cache.h"
#include "storage/kv_store.h"

static int pass = 0, fail = 0;

#define CHECK(expr, msg) \
    do { if (expr) { ++pass; std::cout << "  [PASS] " << msg << "\n"; } \
         else      { ++fail; std::cout << "  [FAIL] " << msg << "\n"; } } while(0)

void testHashMap() {
    std::cout << "\n-- HashMap --\n";
    HashMap m;
    std::string v;

    m.set("name", "alice");
    CHECK(m.get("name", v) && v == "alice",    "set/get basic");
    CHECK(!m.get("missing", v),                "get missing key");
    CHECK(m.exists("name"),                    "exists true");
    CHECK(!m.exists("ghost"),                  "exists false");

    m.set("name", "bob");
    CHECK(m.get("name", v) && v == "bob",      "overwrite key");

    CHECK(m.del("name"),                       "delete existing");
    CHECK(!m.del("name"),                      "delete missing");
    CHECK(!m.get("name", v),                   "get after delete");

    // Fill past rehash threshold
    for (int i = 0; i < 600; ++i) m.set("k" + std::to_string(i), "v");
    CHECK(m.get("k0", v) && v == "v",          "survive rehash");
}

void testLRU() {
    std::cout << "\n-- LRUCache --\n";
    LRUCache c(3);
    std::string v;

    c.put("a", "1"); c.put("b", "2"); c.put("c", "3");
    CHECK(c.get("a", v) && v == "1",  "get existing");
    c.put("d", "4");   // should evict "b" (LRU)
    CHECK(!c.get("b", v),             "LRU eviction");
    CHECK(c.get("d", v),              "new entry after eviction");

    c.evict("a");
    CHECK(!c.get("a", v),             "manual evict");
}

void testKVStore() {
    std::cout << "\n-- KVStore --\n";
    // Use /tmp to avoid cwd AOF file permission issues
    // KVStore writes kvstore.aof in cwd — fine for tests
    KVStore kv;

    CHECK(kv.cmdSet("x", "42") == "+OK\r\n",   "SET returns OK");
    CHECK(kv.cmdGet("x") == "$2\r\n42\r\n",    "GET returns value");
    CHECK(kv.cmdGet("z") == "$-1\r\n",         "GET missing = nil");

    CHECK(kv.cmdDel("x") == ":1\r\n",          "DEL existing");
    CHECK(kv.cmdDel("x") == ":0\r\n",          "DEL missing");
    CHECK(kv.cmdGet("x") == "$-1\r\n",         "GET after DEL");

    CHECK(kv.cmdPing() == "+PONG\r\n",         "PING");
}

void testTTL() {
    std::cout << "\n-- TTL --\n";
    KVStore kv;
    kv.cmdSet("temp", "val");

    CHECK(kv.cmdExpire("temp", 1) == ":1\r\n", "EXPIRE returns 1");
    CHECK(kv.cmdExpire("nope", 1) == ":0\r\n", "EXPIRE missing returns 0");
    CHECK(kv.cmdTTL("temp")       == ":1\r\n" ||
          kv.cmdTTL("temp")       == ":0\r\n", "TTL returns secs");
    CHECK(kv.cmdGet("temp") == "$3\r\nval\r\n","GET before expiry");

    // Wait for expiry (TTL manager fires every 50ms)
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    CHECK(kv.cmdGet("temp") == "$-1\r\n",       "GET after expiry = nil");
}

int main() {
    std::cout << "=== Unit Tests ===\n";
    testHashMap();
    testLRU();
    testKVStore();
    testTTL();
    std::cout << "\nResults: " << pass << " passed, " << fail << " failed\n";
    return fail > 0 ? 1 : 0;
}