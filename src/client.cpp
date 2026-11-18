#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <string>

int main(void){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        perror("socket");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    if(connect(fd, (const struct sockaddr*)&addr, sizeof(addr))){
        perror("connect");
        return -1;
    }
    std::string msg = "hello server";
    send(fd, msg.c_str(), msg.length(), 0);

    char readBuf[128] = {};
    ssize_t n = recv(fd, readBuf, sizeof(readBuf), 0);
    if(n < 0){
        perror("read");
        return -1;
    }
    
    std::cout << "the server says: " << readBuf << "\n";
    close(fd);
    return 0;
}