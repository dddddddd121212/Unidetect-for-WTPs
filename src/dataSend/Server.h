#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

class Server {
public:
    Server(int port);
    ~Server();
    void start(); 
    void sendData(const std::string& data);

private:
    int server_fd;
    int port;
    struct sockaddr_in address;
    int new_socket; 
};

extern Server server;

#endif // SERVER_H
