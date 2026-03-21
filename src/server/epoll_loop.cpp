#include "epoll_loop.h"
#include <unistd.h>
#include <cerrno>
#include <stdexcept>

EpollLoop::EpollLoop() {
    efd = epoll_create1(0);
    if (efd < 0) throw std::runtime_error("epoll_create1 failed");
}

EpollLoop::~EpollLoop() { close(efd); }

bool EpollLoop::add(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events  = events;
    ev.data.fd = fd;
    return epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool EpollLoop::modify(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events  = events;
    ev.data.fd = fd;
    return epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool EpollLoop::remove(int fd) {
    return epoll_ctl(efd, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

void EpollLoop::run(std::function<void(int, uint32_t)> handler) {
    running = true;
    epoll_event events[kMaxEvents];

    while (running) {
        int n = epoll_wait(efd, events, kMaxEvents, 100);  // 100ms timeout
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < n; ++i)
            handler(events[i].data.fd, events[i].events);
    }
}