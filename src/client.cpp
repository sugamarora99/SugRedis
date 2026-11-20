#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

static int32_t read_full(int clientFd, char* buf, ssize_t n){
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

static int32_t write_full(int clientFd, char* msg, ssize_t length){
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

static int32_t send_req(int fd, const std::string& text) {
    uint32_t size = (uint32_t) text.size();
    std::string full_msg;
    full_msg.reserve(4+size);
    full_msg.append(reinterpret_cast<const char*>(&size), 4);
    full_msg += text;
    int error = write_full(fd, full_msg.data(), full_msg.size());
    if(error){
        std::cerr << "error with sending message to server\n";
        return -1;
    }
    return 0;
}

static int32_t read_res(int fd) {
    // read response from server
    // the header is 4 bytes, which is the size of the message
    // process header
    char buf[4 + 4096];
    int error = read_full(fd, buf, 4); //for now assume little endian
    if(error){
        std::cerr << "error fully reading request header\n";
        return error;
    }
    uint32_t len = 0;
    memcpy(&len, buf, 4);
    if(len > 4096){
        std::cerr << "invalid head, message too long\n";
        return -1;
    }

    // process the message body
    error = read_full(fd, &buf[4], len);
    if(error){
        std::cerr << "error fully reading request body\n";
        return error;
    }

    std::cout << "[CLIENT] server said: " << std::string(buf+4, len) << "\n";
    return 0;
}

int main(void){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0){
        perror("socket");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);
    if(connect(fd, (const struct sockaddr*)&addr, sizeof(addr))){
        perror("connect");
        return -1;
    }

    std::string bigMsg(10000, 'x');  // > MAX_MSG_SIZE (4096) to force multiple recv iterations
    std::vector<std::string> requests{"hello1\n", "hello2\n", "hello3\n", bigMsg};

    // send every request first (pipelining)
    for(const auto& req : requests){
        if(send_req(fd, req)){
            std::cerr << "error sending request\n";
            close(fd);
            return -1;
        }
    }

    // then read every response
    for(size_t i = 0; i < requests.size(); ++i){
        if(read_res(fd)){
            std::cerr << "error reading response\n";
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}