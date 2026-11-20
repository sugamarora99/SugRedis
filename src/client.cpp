#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>
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

// encode a command as a RESP array of bulk strings
static std::string encodeCommand(const std::vector<std::string>& args) {
    std::string out = "*" + std::to_string(args.size()) + "\r\n";
    for (const auto& a : args) {
        out += "$" + std::to_string(a.size()) + "\r\n" + a + "\r\n";
    }
    return out;
}

// read bytes until \r\n (inclusive), return the line without \r\n
static std::string readLine(int fd) {
    std::string line;
    char c;
    while (read_full(fd, &c, 1) == 0) {
        if (c == '\r') { read_full(fd, &c, 1); return line; }  // consume '\n'
        line += c;
    }
    return line;
}

static int32_t send_req(int fd, const std::vector<std::string>& args) {
    std::string msg = encodeCommand(args);
    int error = write_full(fd, msg.data(), msg.size());
    if(error){
        std::cerr << "error with sending message to server\n";
        return -1;
    }
    return 0;
}

static int32_t read_res(int fd) {
    char type;
    if(read_full(fd, &type, 1) != 0){
        std::cerr << "error reading reply type\n";
        return -1;
    }

    std::string line = readLine(fd);
    switch(type){
        case '+':
            std::cout << line << "\n";
            break;
        case '-':
            std::cout << "(error) " << line << "\n";
            break;
        case ':':
            std::cout << "(integer) " << line << "\n";
            break;
        case '$': {
            long len = std::strtol(line.c_str(), nullptr, 10);
            if(len < 0){
                std::cout << "(nil)\n";
                break;
            }
            std::string body((size_t)len, '\0');
            if(read_full(fd, body.data(), len) != 0){
                std::cerr << "error reading bulk string body\n";
                return -1;
            }
            char crlf[2];
            if(read_full(fd, crlf, 2) != 0){
                std::cerr << "error reading bulk string terminator\n";
                return -1;
            }
            std::cout << body << "\n";
            break;
        }
        default:
            std::cerr << "unknown reply type '" << type << "'\n";
            return -1;
    }
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

    std::vector<std::vector<std::string>> commands = {
        {"PING"},
        {"ECHO", "hello world"},
        {"SET", "foo", "bar"},
        {"GET", "foo"},
        {"ECHO", std::string(10000, 'x')},  // large payload to exercise multi-recv
    };

    // send every request first (pipelining)
    for(const auto& cmd : commands){
        if(send_req(fd, cmd)){
            std::cerr << "error sending request\n";
            close(fd);
            return -1;
        }
    }

    // then read every response
    for(size_t i = 0; i < commands.size(); ++i){
        if(read_res(fd)){
            std::cerr << "error reading response\n";
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}
