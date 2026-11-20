#include "server.h"
#include <stdio.h>
#include <netinet/in.h>
#include <iostream>
#include <string>
#include <fcntl.h>
#include <sys/socket.h>


namespace SugRedis {

constexpr size_t MAX_MSG_SIZE = 4096;
constexpr size_t kMaxMessageSize = 64 * 1024;

bool Server::init(){
    //IPv4 + TCP
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(serverFd_ < 0){
        perror("socket");
        return false;
    }

    // in case server crashes, it should be able to bind to the same ip/port after restart
    int val = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    
    // use non blocking socket operations
    int flags = fcntl(serverFd_, F_GETFL, 0);
    fcntl(serverFd_, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);

    // When a service binds to 0.0.0.0, it listens for incoming connections on
    // all available network interfaces (e.g., Ethernet, Wi-Fi, loopback) simultaneously,
    // making the service accessible from any local or external IP address
    addr.sin_addr.s_addr = htonl(0);
    if(bind(serverFd_, (const struct sockaddr*)&addr, sizeof(addr))){
        perror("bind");
        return false;
    }

    if(listen(serverFd_, SOMAXCONN)){
        perror("listen");
        return false;
    }

    // when client connects to a server, the server is ready to be read
    poller.add(serverFd_, true, false);
    return true;
}

void Server::handleIncomingConnections(){
    while(true){
        // accept incoming connections
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int clientFd = accept(serverFd_, (struct sockaddr*)&client_addr, &addrlen);
        if(clientFd < 0){
            // non blocking accept() returns EAGAIN if nothing in connection queue
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("accept");
            }
            break;
        }

        //make client socket nonblocking
        int flags = fcntl(clientFd, F_GETFL, 0);
        fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);

        //add clientFd to interest list so that we are notified when they have events
        poller.add(clientFd, true, false);

        // make sure that clientFd is valid index in clients "map"
        if((size_t) clientFd >= clients.size()){
            clients.resize(clientFd+1);
        }
        clients[clientFd] = std::make_unique<ClientState>(clientFd);
    }
}

void Server::disconnectClient(int fd){
    std::cerr << "disconnecting client " << fd << " due to " << std::strerror(errno) << "\n";
    poller.del(fd);
    ::close(fd);
    clients[fd].reset();
}

// processes all data in request buffer for a given client
void Server::parseBatch(int fd){
    auto& requestVec = clients[fd]->requestData;
    if(requestVec.size() < 4){
        //not enough info to process the request, header is incomplete
        return;
    }

    bool processed = false;

    //process headers first
    size_t i = 0;
    while(i+4 <= requestVec.size()){
        uint32_t len = 0;
        memcpy(&len, &requestVec[i], 4);
        if(len > kMaxMessageSize){
            std::cerr << "Invalid request length beyond max size for client " << fd << "\n";
            disconnectClient(fd);
            return;
        }
        if(requestVec.size() >= i + 4 + len){
            // we have recieved the complete request
            std::string responseBody = "The server has recieved your message of length " + std::to_string(len) + "\n";
            uint32_t responseLen = (uint32_t) responseBody.size();
            auto& resp = clients[fd]->responseData;
            resp.reserve(resp.size() + 4 + responseLen);
            //add header to buffer
            resp.insert(resp.end(),
                        reinterpret_cast<uint8_t*>(&responseLen),
                        reinterpret_cast<uint8_t*>(&responseLen) + 4);
            // add body to buffer
            resp.insert(resp.end(), responseBody.begin(), responseBody.end());

            i += 4 + len;
            processed = true;
        } else{
            // we have not recived all of the data for this request yet
            break;
        }
    }

    if (i > 0){
        //remove procesed requests from buffer
        requestVec.erase(requestVec.begin(), requestVec.begin() + i);
    }

    if (processed) {
        clients[fd]->state = ProcessingState::Writing;
        clients[fd]->responseCursor = 0;
        poller.mod(fd, false, true);
    }
}

void Server::handleClientRequest(int fd){
    // since we are using nonblocking sockets, for a client, the server can be in the middle of reading the message OR
    // in the middle of writing response to the client
    
    // it is possible that a single event doesn't return the complete message from client
    // ^^ this logic extends to even the 4 byte header
    while(true){
        char buf[MAX_MSG_SIZE];
        // read as much data from client as possible. (until EAGAIN)
        // client can send multiple requests in one go (pipelining)
        if(clients[fd]->state != ProcessingState::Writing){
            auto& dataVec = clients[fd]->requestData;
            ssize_t bytesRead = recv(fd, buf, sizeof(buf), 0);
            if(bytesRead > 0){
                dataVec.insert(dataVec.end(), buf, buf+ bytesRead);
            }
            else if(bytesRead == 0){    //client has disconnected
                // close client fd, reset() in vector
                // parseBatch(fd);
                ::close(fd);
                clients[fd].reset();
                poller.del(fd);
                break;
            }
            else if(errno == EAGAIN || errno == EWOULDBLOCK){
                // we have read all that we can
                // now parse all the request data we have for complete requests
                parseBatch(fd);
                break;
            } else {
                // genuine error
                disconnectClient(fd);
                break;
            }
        } else{
            // flow for handling response
            auto& response = clients[fd]->responseData;

            if(clients[fd]->responseCursor > response.size()){
                std::cerr << "BUG response cursor for client " << fd << " has gone too far\n";
                disconnectClient(fd);
                break;
            }

            // continue sending response
ssize_t bytesWritten = send(fd, reinterpret_cast<const char*>(response.data() + clients[fd]->responseCursor), response.size() - clients[fd]->responseCursor, 0);
            if(bytesWritten < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    break;
                } else{
                    disconnectClient(fd);
                    break;
                }
            } else if(bytesWritten == 0){
                disconnectClient(fd);
                break;
            } else{
                clients[fd]->responseCursor += bytesWritten;
                // have we written the entire response already?
                if(clients[fd]->responseCursor == response.size()){
                    clients[fd]->state = ProcessingState::Reading;
                    clients[fd]->responseCursor = 0;
                    clients[fd]->responseData.clear();
                    poller.mod(fd, true, false);
                    break;
                }
            }
        }
    }
}

//TODO: add RESP2 (Redis protocol) parsing
// core event loop
void Server::run(){
    while(1){
        SugRedis::Event events[SugRedis::kMaxEvents];
        
        //block until at least one fd is ready
        int eventCount = poller.wait(events, kMaxEvents, -1);
        if(eventCount < 0){
            // wait() can be interrupted
            if(errno == EINTR){
                // just try again
                continue;
            }
        }

        //process events
        for(int i = 0; i < eventCount; i++){
            if(events[i].fd == serverFd_){
                handleIncomingConnections();
            } else{
                handleClientRequest(events[i].fd);
            }
        }
    }
}



};  // namespace SugRedis

int main(void){
    SugRedis::Server server;
    if(!server.init()){
        std::cout << "server failed to init\n";
    }

    server.run();
    return 0;
}