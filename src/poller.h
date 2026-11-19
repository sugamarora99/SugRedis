#ifndef POLLER_H
#define POLLER_H

namespace SugRedis {

constexpr int kMaxEvents = 64;

struct Event {
    int fd;
    bool readable;
    bool writable;
    bool error;
};

class Poller {
public:
    Poller();
    ~Poller();

    Poller(const Poller&) = delete;
    Poller& operator=(const Poller&) = delete;

    bool valid() const { return fd_ >= 0; }

    bool add(int fd, bool wantRead, bool wantWrite);
    bool mod(int fd, bool wantRead, bool wantWrite);
    bool del(int fd);

    int wait(Event* out, int maxEvents, int timeoutMs);

private:
    int fd_;
};

}  // namespace SugRedis

#endif
