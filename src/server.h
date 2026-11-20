#ifndef SERVER_H
#define SERVER_H
#include <vector>
#include "poller.h"
#include <unistd.h>

namespace SugRedis {
    enum class ProcessingState{
        Reading,
        Writing
    };

    struct ClientState {
        int fd_;
        std::vector<uint8_t> requestData;
        uint32_t requestLength = 0;
        
        std::vector<uint8_t> responseData;
        size_t responseCursor = 0;  //points to next byte of response to be written

        ProcessingState state = ProcessingState::Reading;
        ClientState(int fd) : fd_(fd){}
    };

    class Server {
        public:
        Server() : serverFd_(-1) {}
        ~Server(){
            if(serverFd_ >= 0){
                ::close(serverFd_);
            }
        }

        bool init();    // create/bind/listen server socket, setup epoll/kqueue
        void run();     // loop


        private:
        void parseBatch(int fd);
        void handleIncomingConnections();
        void handleClientRequest(int fd);
        void disconnectClient(int fd);

        int serverFd_;
        Poller poller;

        // vector is optimal because max fd value is bounded by the # of fds currently open
        std::vector<std::unique_ptr<ClientState>> clients;
    };

};  // namespace SugRedis

#endif