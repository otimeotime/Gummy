#include "../client/network/ClientSocket.hpp"
#include <iostream>
#include <thread>
#include <chrono>

void PrintStep(int step, const std::string& description) {
    std::cout << "\n--- Step " << step << ": " << description << " ---" << std::endl;
}

int main() {
    ClientSocket client;
    std::string ip = "127.0.0.1";
    int port = 8080;

    std::cout << "Connecting to server at " << ip << ":" << port << "..." << std::endl;
    if (!client.Connect(ip, port)) {
        std::cerr << "Failed to connect to server. Ensure server is running." << std::endl;
        return -1;
    }
    std::cout << "Connected!" << std::endl;

    // 1. Register with username: test, password: 123
    PrintStep(1, "Register (test, 123)");
    std::string regResult = client.Register("test", "123");
    std::cout << "Result: " << regResult << std::endl;

    // 2. Login
    PrintStep(2, "Login (test, 123)");
    std::string loginResult = client.Login("test", "123");
    std::cout << "Result: " << loginResult << std::endl;

    if (loginResult == "Success" || loginResult.find("Welcome") != std::string::npos) {
        // 3. Change password to password: 456
        PrintStep(3, "Change Password (old: 123, new: 456)");
        bool changePwResult = client.ChangePassword("123", "456");
        
        if (changePwResult) {
            std::cout << "Result: Success" << std::endl;
        } else {
            std::cout << "Result: Failed" << std::endl;
        }

        // Optional: Verify login with new password
        // PrintStep(3.5, "Verify Login with NEW password (test, 456)");
        // std::cout << "Result: " << client.Login("test", "456") << std::endl;
    } else {
        std::cerr << "Skipping password change because login failed." << std::endl;
    }

    // 4. Logout
    PrintStep(4, "Logout");
    client.Logout();
    std::cout << "Logged out." << std::endl;

    // Cleanup
    client.Disconnect();
    return 0;
}