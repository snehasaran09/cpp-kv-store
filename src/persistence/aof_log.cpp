#include "persistence/aof_log.h"
#include <fstream>
#include <sstream>
#include <iostream>

AOFLog::AOFLog(const std::string& path) : path(path) {
    ofs.open(path, std::ios::app);
    if (!ofs.is_open())
        std::cerr << "[AOF] Warning: cannot open " << path << "\n";
}

AOFLog::~AOFLog() { if (ofs.is_open()) ofs.close(); }

void AOFLog::logSet(const std::string& key, const std::string& val) {
    std::lock_guard<std::mutex> lk(mu);
    ofs << "SET " << key << " " << val << "\n";
    ofs.flush();
}

void AOFLog::logDel(const std::string& key) {
    std::lock_guard<std::mutex> lk(mu);
    ofs << "DEL " << key << "\n";
    ofs.flush();
}

void AOFLog::logExpire(const std::string& key, long long ttlMs) {
    std::lock_guard<std::mutex> lk(mu);
    ofs << "EXPIRE " << key << " " << ttlMs << "\n";
    ofs.flush();
}

void AOFLog::replay(
    std::function<void(const std::string&, const std::string&)> onSet,
    std::function<void(const std::string&)>                     onDel,
    std::function<void(const std::string&, long long)>          onExpire
) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return;   // no log file yet — first run

    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd == "SET") {
            std::string key, val;
            iss >> key;
            std::getline(iss >> std::ws, val);  // value may contain spaces
            onSet(key, val);
        } else if (cmd == "DEL") {
            std::string key; iss >> key;
            onDel(key);
        } else if (cmd == "EXPIRE") {
            std::string key; long long ttlMs;
            iss >> key >> ttlMs;
            onExpire(key, ttlMs);
        }
    }
}