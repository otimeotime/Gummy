#include "TCPSocket.hpp"
#include <cstring>
#include <iostream>

TCPSocket::TCPSocket() {
    mSockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (mSockFd < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    std::memset(&mAddr, 0, sizeof(mAddr));
}

TCPSocket::TCPSocket(int sockfd, struct sockaddr_in addr)
    : mSockFd(sockfd), mAddr(addr) {}

TCPSocket::~TCPSocket() {
    Close();
}

void TCPSocket::Close() {
    if (mSockFd >= 0) {
        close(mSockFd);
        mSockFd = -1;
    }
}

void TCPSocket::Bind(int port) {
    mAddr.sin_family = AF_INET;
    mAddr.sin_addr.s_addr = INADDR_ANY;
    mAddr.sin_port = htons(port);

    int opt = 1;

    if (setsockopt(mSockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Set socket options failed");
    }

    if (bind(mSockFd, (struct sockaddr*)&mAddr, sizeof(mAddr)) < 0) {
        throw std::runtime_error("Bind failed");
    }
}

void TCPSocket::Listen(int backlog) {
    if (listen(mSockFd, backlog) < 0) {
        throw std::runtime_error("Listen failed");
    }
}

TCPSocket* TCPSocket::Accept() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    int clientFd = accept(mSockFd, (struct sockaddr*)&clientAddr, &clientLen);

    if (clientFd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return nullptr; // No pending connections
        }
        return nullptr;
    }
    return new TCPSocket(clientFd, clientAddr);
}

void TCPSocket::Connect(const std::string& ip, int port) {
    mAddr.sin_family = AF_INET;
    mAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &mAddr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid address");
    }

    if (connect(mSockFd, (struct sockaddr*)&mAddr, sizeof(mAddr)) < 0) {
        throw std::runtime_error("Connection failed");
    }
}

bool TCPSocket::Send(const void* data, size_t size) {
    size_t totalSent = 0;
    const char* dataPtr = static_cast<const char*>(data);
    while (totalSent < size) {
        ssize_t sent = send(mSockFd, dataPtr + totalSent, size - totalSent, 0);
        if (sent < 0) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}

int TCPSocket::Receive(void* buffer, size_t size) {
    ssize_t bytesRead = recv(mSockFd, buffer, size, 0);
    if (bytesRead < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return -1; 
        }
        return 0;
    }
    return static_cast<int>(bytesRead);
}

void TCPSocket::SetNonBlocking(bool isNonBlocking) {
    int flags = fcntl(mSockFd, F_GETFL, 0);
    if (flags == -1) return;
    if (isNonBlocking) {
        fcntl(mSockFd, F_SETFL, flags | O_NONBLOCK);
    } else {
        fcntl(mSockFd, F_SETFL, flags & ~O_NONBLOCK);
    }
}
