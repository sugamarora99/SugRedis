#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__) || defined(__DragonFly__)

#include "poller.h"

#include <sys/event.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>

namespace SugRedis {

Poller::Poller() : fd_(-1) {
    fd_ = ::kqueue();
    if (fd_ < 0) {
        std::cerr << "kqueue: " << std::strerror(errno) << "\n";
    }
}

Poller::~Poller() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

static bool apply(int kq, int fd, bool wantRead, bool wantWrite) {
    struct kevent changes[2];
    int n = 0;

    if (wantRead) {
        EV_SET(&changes[n++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    } else {
        EV_SET(&changes[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    }

    if (wantWrite) {
        EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    } else {
        EV_SET(&changes[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    }

    if (::kevent(kq, changes, n, nullptr, 0, nullptr) < 0) {
        // ENOENT is benign: we asked to delete a filter that wasn't registered.
        if (errno != ENOENT) {
            std::cerr << "kevent: " << std::strerror(errno) << "\n";
            return false;
        }
    }
    return true;
}

bool Poller::add(int fd, bool wantRead, bool wantWrite) {
    return apply(fd_, fd, wantRead, wantWrite);
}

bool Poller::mod(int fd, bool wantRead, bool wantWrite) {
    return apply(fd_, fd, wantRead, wantWrite);
}

bool Poller::del(int fd) {
    return apply(fd_, fd, false, false);
}

int Poller::wait(Event* out, int maxEvents, int timeoutMs) {
    struct timespec* ts = nullptr;
    struct timespec timeout{};
    if (timeoutMs >= 0) {
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_nsec = (timeoutMs % 1000) * 1000000;
        ts = &timeout;
    }

    struct kevent events[kMaxEvents];
    int n = ::kevent(fd_, nullptr, 0, events, kMaxEvents, ts);
    if (n < 0) {
        if (errno != EINTR) {
            std::cerr << "kevent: " << std::strerror(errno) << "\n";
        }
        return -1;
    }

    int count = 0;
    for (int i = 0; i < n && count < maxEvents; ++i) {
        //initializing an object with prvalue of same type guarantees copy elision
        out[count++] = Event{
            static_cast<int>(events[i].ident),
            events[i].filter == EVFILT_READ,
            events[i].filter == EVFILT_WRITE,
            (events[i].flags & (EV_EOF | EV_ERROR)) != 0
        };
    }
    return count;
}

}  // namespace SugRedis

#endif
