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
    void start(); //启动服务端并等待客户端连接
    void sendData(const std::string& data);//向客户端发送数据

private:
    int server_fd;
    int port;
    struct sockaddr_in address;
    int new_socket; //保存与客户端的连接socket
};

#endif // SERVER_H
