#include "core/GameServer.hpp"

#include <csignal>
#include <iostream>

namespace {
GameServer* g_server = nullptr;

void HandleSigInt(int) {
    if (g_server) {
        std::cout << "\nSIGINT: stopping GameServer..." << std::endl;
        g_server->Stop();
    }
}
}

int main(int argc, char** argv) {
    int port = 9090;
    if (argc >= 2) {
        port = std::atoi(argv[1]);
        if (port <= 0) port = 9090;
    }

    GameServer server;
    g_server = &server;
    std::signal(SIGINT, HandleSigInt);

    server.Run(port);

    g_server = nullptr;
    return 0;
}
