#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <functional>

// Append-Only File persistence (Redis-style).
// Every mutating command is flushed to disk immediately.
// On startup, replay() reconstructs state from the log.
class AOFLog {
public:
    explicit AOFLog(const std::string& path = "kvstore.aof");
    ~AOFLog();

    void logSet   (const std::string& key, const std::string& val);
    void logDel   (const std::string& key);
    void logExpire(const std::string& key, long long ttlMs);

    void replay(
        std::function<void(const std::string&, const std::string&)> onSet,
        std::function<void(const std::string&)>                     onDel,
        std::function<void(const std::string&, long long)>          onExpire
    );

private:
    std::string   path;
    std::ofstream ofs;
    mutable std::mutex mu;
};
