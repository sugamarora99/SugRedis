#include "server.h"
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <string>


namespace SugRedis {

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
    addr.sin_addr.s_addr = htonl(0);    // wildcard IP 0.0.0.0
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

        do_something(clientFd);
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