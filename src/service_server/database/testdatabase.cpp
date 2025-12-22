#include <iostream>
#include "DatabaseServer.hpp"
#include "UserDAO.hpp"

int main() {
    // 1. Create the DatabaseServer instance
    DatabaseServer* db = new DatabaseServer("gummydatabase", "postgres", "Hehehe123");

    // 2. EXPLICITLY CALL CONNECT
    if (db->connect()) {
        
        // 3. Pass the connected server to the DAO
        UserDAO userDao(db);

        // 4. Perform operations
        long userId = userDao.createUser("testuser", "testpass");
        if (userId != -1) {
            std::cout << "User created with ID: " << userId << std::endl;
        }

        auto user = userDao.authenticate("testuser", "testpass");
        if (user) {
            std::cout << "User authenticated: " << user->username << " with ELO " << user->elo << std::endl;
        } else {
            std::cout << "Authentication failed." << std::endl;
        }

        userDao.updateElo(userId, 350);
        userDao.updateUsername(userId, "updateduser");
        userDao.updatePassword(userId, "newpass");
        auto updatedUser = userDao.getUserById(userId);
        if (updatedUser) {
            std::cout << "Updated ELO: " << updatedUser->elo << std::endl;
            std::cout << "Updated Username: " << updatedUser->username << std::endl;
            std::cout << "Updated Password: " << updatedUser->password << std::endl;
        }

    } else {
        std::cerr << "Failed to connect to database. Check credentials or if Postgres is running." << std::endl;
    }

    // Clean up
    delete db;
    return 0;
}