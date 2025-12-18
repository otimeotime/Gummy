#ifndef TCP_SOCKET_HPP
#define TCP_SOCKET_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdexcept>
#include <vector>

class TCPSocket {
private:
    int mSockFd;
    struct sockaddr_in mAddr;

public:
    // Constructor & Destructor
    TCPSocket();
    TCPSocket(int sockfd, struct sockaddr_in addr);
    ~TCPSocket();
    // Server Methods
    void Bind(int port);
    void Listen(int backlog = 10);
    TCPSocket* Accept();
    // Client Methods
    void Connect(const std::string& ip, int port);
    // Communication Methods
    bool Send(const void* data, size_t size);
    int Receive(void* buffer, size_t size);
    // Utility Methods
    void Close();
    void SetNonBlocking(bool isNonBlocking);
    int GetFd() const { return mSockFd; }
    bool IsValid() const { return mSockFd >= 0;}
};
#endif // TCP_SOCKET_HPP