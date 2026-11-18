#include "server.h"
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <string>


namespace SugRedis {

constexpr size_t MAX_MSG_SIZE = 4096;

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
    
    return true;
}

static void do_something(int clientFd){
    std::string buf(128, '\0');
    ssize_t n = recv(clientFd, buf.data(), buf.size(), 0);
    if(n < 0){
        std::cerr << "read() error\n";
        return;
    }

    std::cout << "client says: " << buf << "\n";
    std::string writeBuf = "from server: what up client?";
    send(clientFd, writeBuf.c_str(), writeBuf.length(), 0);
}

static int32_t read_full(int clientFd, char* buf, size_t n){
    while(n > 0){
        ssize_t recieved = recv(clientFd, buf, n, 0);
        if(recieved <= 0){
            return -1;  //error or unexpected EOF
        }
        else if(recieved > n){
            throw std::runtime_error(std::strerror(errno));  // release: handle it
        }
       
        n -= (size_t) recieved;
        buf += recieved;
    }
    return 0;
}

static int32_t write_full(int clientFd, char* msg, size_t length){
    while(length > 0){
        ssize_t written = send(clientFd, msg, length, 0);
        if(written <= 0){
            return -1;  //error
        }
        else if(written > length){
            throw std::runtime_error(std::strerror(errno));
        }

        length -= (size_t) written;
        msg += written;
    }
    return 0;
}

static int32_t one_request(int clientFd){
    // the header is 4 bytes, which is the size of the message
    // process header
    char buf[4 + MAX_MSG_SIZE];
    int32_t error = read_full(clientFd, buf, 4); //for now assume little endian
    if(error){
        std::cerr << "error fully reading request header\n";
        return error;
    }
    uint32_t len = 0;
    memcpy(&len, buf, 4);
    if(len > MAX_MSG_SIZE){
        std::cerr << "invalid head " << len << ", message too long\n";
        return -1;
    }

    // process the message body
    error = read_full(clientFd, &buf[4], len);
    if(error){
        std::cerr << "error fully reading request body\n";
        return error;
    }
    
    std::cout << "[SERVER] client said: " << std::string(buf+4, len) << "\n";

    // send response
    std::string response = "Nice message, client!\n";
    std::string writeBuf;
    writeBuf.reserve(4+response.size());
    len = (uint32_t) response.size();
    writeBuf.append(reinterpret_cast<const char*>(&len), 4);
    writeBuf += response;
    return write_full(clientFd, writeBuf.data(), writeBuf.size());
}

void Server::run(){
    while(1){
        // accept incoming connections
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int clientFd = accept(serverFd_, (struct sockaddr*)&client_addr, &addrlen);
        if(clientFd < 0){
            perror("accept");
            break;
        }
        while(1){   //for now let's serve only one connection
            int32_t error = one_request(clientFd);
            if(error){
                std::cout << "error processing client request\n";
                break;
            }
        }
        close(clientFd);
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