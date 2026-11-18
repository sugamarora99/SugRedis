#ifndef SERVER_H
#define SERVER_H

namespace SugRedis {

    class Server {
        public:
        Server() : serverFd_(-1) {}

        bool init();    // create/bind/listen server socket, setup epoll/kqueue
        void run();     // loop


        private:
        int serverFd_;
    };

};  // namespace SugRedis

#endif