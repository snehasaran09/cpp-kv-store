#include "tcp_server.h"
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <stdexcept>

TCPServer::TCPServer(int port, KVStore& kv) : port(port), sfd(-1), kv(kv) {
    if (!initSocket()) throw std::runtime_error("Socket setup failed");
}

TCPServer::~TCPServer() {
    stop();
    if (sfd >= 0) close(sfd);
}

bool TCPServer::initSocket() {
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) return false;

    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port));

    if (bind(sfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) return false;
    if (listen(sfd, 512) < 0) return false;

    setNonBlocking(sfd);
    loop.add(sfd, EPOLLIN);
    std::cout << "[Server] Listening on port " << port << "\n";
    return true;
}

void TCPServer::setNonBlocking(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

void TCPServer::start() {
    loopThread = std::thread(&TCPServer::runLoop, this);
}

void TCPServer::stop() {
    loop.stop();
    if (loopThread.joinable()) loopThread.join();
}

void TCPServer::runLoop() {
    loop.run([this](int fd, uint32_t ev) {
        if (fd == sfd) {
            acceptAll();
        } else if (ev & (EPOLLHUP | EPOLLERR)) {
            closeClient(fd);
        } else if (ev & EPOLLIN) {
            readClient(fd);
        }
    });
}

void TCPServer::acceptAll() {
    while (true) {
        int cfd = accept(sfd, nullptr, nullptr);
        if (cfd < 0) break;   // EAGAIN: no more pending connections
        setNonBlocking(cfd);
        loop.add(cfd, EPOLLIN | EPOLLET);
        std::lock_guard<std::mutex> lk(bufMu);
        buffers[cfd] = "";
    }
}

void TCPServer::readClient(int cfd) {
    char buf[4096];
    std::string& rbuf = buffers[cfd];

    // Drain the socket (edge-triggered: must read until EAGAIN)
    while (true) {
        ssize_t n = read(cfd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            closeClient(cfd); return;
        }
        if (n == 0) { closeClient(cfd); return; }   // client disconnected
        rbuf.append(buf, static_cast<size_t>(n));
    }

    // Process complete newline-delimited commands
    size_t pos;
    while ((pos = rbuf.find('\n')) != std::string::npos) {
        std::string line = rbuf.substr(0, pos);
        rbuf.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();  // strip \r
        if (line.empty()) continue;
        std::string resp = dispatch(line);
        write(cfd, resp.data(), resp.size());
    }
}

void TCPServer::closeClient(int cfd) {
    loop.remove(cfd);
    close(cfd);
    std::lock_guard<std::mutex> lk(bufMu);
    buffers.erase(cfd);
}

std::string TCPServer::dispatch(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    for (char& c : cmd)
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

    if (cmd == "PING") return kv.cmdPing();

    if (cmd == "SET") {
        std::string key, val;
        iss >> key;
        std::getline(iss >> std::ws, val);
        return kv.cmdSet(key, val);
    }
    if (cmd == "GET") {
        std::string key; iss >> key;
        return kv.cmdGet(key);
    }
    if (cmd == "DEL") {
        std::string key; iss >> key;
        return kv.cmdDel(key);
    }
    if (cmd == "EXPIRE") {
        std::string key; long long secs;
        iss >> key >> secs;
        return kv.cmdExpire(key, secs);
    }
    if (cmd == "TTL") {
        std::string key; iss >> key;
        return kv.cmdTTL(key);
    }
    return "-ERR unknown command '" + cmd + "'\r\n";
}