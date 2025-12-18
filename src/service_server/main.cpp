#include "core/ServiceServer.hpp"
#include <iostream>
#include <csignal>

// Global pointer for signal handling to stop server gracefully
ServiceServer* g_Server = nullptr;

void SignalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Stopping server..." << std::endl;
    if (g_Server) {
        g_Server->Stop();
    }
}

int main() {
    // Register signal handler for Ctrl+C
    signal(SIGINT, SignalHandler);

    std::cout << "Initializing Service Server..." << std::endl;
    
    // NOTE: Ensure ServiceServer.cpp constructor initializes AuthServer/UserDAO/DatabaseServer 
    // with valid credentials (e.g., "gummydatabase", "postgres", "Hehehe123")
    ServiceServer server;
    g_Server = &server;

    // Run the server on port 8080
    // This function blocks until the server is stopped
    server.Run(8080);

    std::cout << "Server stopped. Goodbye!" << std::endl;
    return 0;
}