#include "core/GameServer.hpp"

#include <csignal>
#include <iostream>
#include <string>

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
    uint32_t matchId = 0;
    std::string recordPath;
    std::string replayPath;

    // Args:
    //   ingame_server_demo [--port N] [--match-id ID] [--record path.grpl]
    //   ingame_server_demo [--port N] --replay path.grpl
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
            if (port <= 0) port = 9090;
        } else if (a == "--match-id" && i + 1 < argc) {
            matchId = (uint32_t)std::strtoul(argv[++i], nullptr, 10);
        } else if (a == "--record" && i + 1 < argc) {
            recordPath = argv[++i];
        } else if (a == "--replay" && i + 1 < argc) {
            replayPath = argv[++i];
        } else if (a == "-h" || a == "--help") {
            std::cout << "Usage:\n"
                      << "  " << argv[0] << " [--port N] [--match-id ID] [--record out.grpl]\n"
                      << "  " << argv[0] << " [--port N] --replay in.grpl\n";
            return 0;
        } else {
            // Back-compat: first positional arg = port
            if (a.size() && a[0] != '-') {
                port = std::atoi(a.c_str());
                if (port <= 0) port = 9090;
            }
        }
    }

    GameServer server;
    if (matchId != 0) {
        server.SetMatchId(matchId);
    }
    if (!recordPath.empty()) {
        server.EnableRecording(recordPath);
    }
    if (!replayPath.empty()) {
        server.EnableReplay(replayPath);
    }
    g_server = &server;
    std::signal(SIGINT, HandleSigInt);

    server.Run(port);

    g_server = nullptr;
    return 0;
}
