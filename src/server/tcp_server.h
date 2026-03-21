#pragma once
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include "epoll_loop.h"
#include "storage/kv_store.h"

// TCP server: epoll-driven, non-blocking, edge-triggered.
// One event-loop thread handles ALL clients.
// Per-client receive buffers accumulate data until a '\n'-terminated command arrives.
class TCPServer {
public:
    TCPServer(int port, KVStore& kv);
    ~TCPServer();

    void start();
    void stop();

private:
    int      port;
    int      sfd;         // listening socket
    KVStore& kv;
    EpollLoop loop;

    std::thread  loopThread;
    std::unordered_map<int, std::string> buffers;  // fd -> recv buffer
    std::mutex bufMu;

    bool initSocket();
    void setNonBlocking(int fd);
    void runLoop();
    void acceptAll();
    void readClient(int cfd);
    void closeClient(int cfd);
    std::string dispatch(const std::string& line);
};