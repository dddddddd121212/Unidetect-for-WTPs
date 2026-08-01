#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Server.h"
#include <arpa/inet.h>

Server::Server(int port) : port(port), new_socket(-1) { 
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket creation failed!" << std::endl;
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
	//address.sin_addr.s_addr = inet_addr("192.168.1.22");
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed!" << std::endl;
        exit(EXIT_FAILURE);
    }
    listen(server_fd, 3);
}

Server::~Server() {
    close(server_fd);
    if(new_socket != -1){ 
    		close(new_socket);
    }
}

void Server::start() {
    int addrlen = sizeof(address);
    
    std::cout << "Starting server..." << std::endl;

    std::cout << "Waiting for client connection..." << std::endl;
    new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
	std::cout << "!!!new_socket" << new_socket << std::endl;

    if (new_socket < 0) {
        std::cout << "Accept failed!" << std::endl;
        perror("Accept error");
        exit(EXIT_FAILURE);
    } else {
        std::cout << "Client connected! Socket fd:" << new_socket << std::endl;
    }

}

void Server::sendData(const std::string& data){
    if(new_socket != -1){
        int send_result = send(new_socket, data.c_str(), data.length(), 0);
	if(send_result < 0){
	    perror("Send error");
	}else {
	   // std::cout << data << std::endl;
	}
    }else {
        std::cerr << "No client connected. Data not sent." << std::endl;
    }
}


