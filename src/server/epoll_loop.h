#pragma once
#include <functional>
#include <atomic>
#include <sys/epoll.h>

// Thin wrapper around Linux epoll.
// Edge-triggered mode (EPOLLET) + non-blocking sockets = maximum throughput.
class EpollLoop {
public:
    EpollLoop();
    ~EpollLoop();

    bool add   (int fd, uint32_t events);
    bool modify(int fd, uint32_t events);
    bool remove(int fd);

    // Blocks until stop() is called; calls handler for every ready fd.
    void run(std::function<void(int fd, uint32_t events)> handler);
    void stop() { running = false; }

private:
    int efd;
    std::atomic<bool> running{false};
    static constexpr int kMaxEvents = 1024;
};