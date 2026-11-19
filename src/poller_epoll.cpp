#if defined(__linux__)

#include "poller.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace SugRedis {

Poller::Poller() : fd_(-1) {
    fd_ = ::epoll_create1(0);
    if (fd_ < 0) {
        std::cerr << "epoll_create1: " << std::strerror(errno) << "\n";
    }
}

Poller::~Poller() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

static uint32_t toEpoll(bool wantRead, bool wantWrite) {
    uint32_t events = 0;
    if (wantRead) {
        events |= EPOLLIN;
    }
    if (wantWrite) {
        events |= EPOLLOUT;
    }
    return events;
}

bool Poller::add(int fd, bool wantRead, bool wantWrite) {
    struct epoll_event ev{};
    ev.events = toEpoll(wantRead, wantWrite);
    ev.data.fd = fd;
    if (::epoll_ctl(fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
        std::cerr << "epoll_ctl(add): " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

bool Poller::mod(int fd, bool wantRead, bool wantWrite) {
    struct epoll_event ev{};
    ev.events = toEpoll(wantRead, wantWrite);
    ev.data.fd = fd;
    if (::epoll_ctl(fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
        std::cerr << "epoll_ctl(mod): " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

bool Poller::del(int fd) {
    if (::epoll_ctl(fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        // EBADF is benign: the fd is already closed or was never registered,
        // so there is nothing to delete.
        if (errno != EBADF) {
            std::cerr << "epoll_ctl(del): " << std::strerror(errno) << "\n";
        }
        return false;
    }
    return true;
}

int Poller::wait(Event* out, int maxEvents, int timeoutMs) {
    struct epoll_event events[kMaxEvents];
    int n = ::epoll_wait(fd_, events, kMaxEvents, timeoutMs);
    if (n < 0) {
        if (errno != EINTR) {
            std::cerr << "epoll_wait: " << std::strerror(errno) << "\n";
        }
        return -1;
    }

    int count = 0;
    for (int i = 0; i < n && count < maxEvents; ++i) {
        //initializing an object with prvalue of same type guarantees copy elision
        out[count++] = Event{
            events[i].data.fd,
            (events[i].events & EPOLLIN) != 0,
            (events[i].events & EPOLLOUT) != 0,
            (events[i].events & (EPOLLERR | EPOLLHUP)) != 0
        };
    }
    return count;
}

}  // namespace SugRedis

#endif
