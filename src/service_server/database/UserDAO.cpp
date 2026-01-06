#include "UserDAO.hpp"

UserDAO::UserDAO(DatabaseServer* database) : db(database) {}

bool UserDAO::deleteUser(const u_int32_t userid) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    try {
        pqxx::work W(*db->getConnection());
        std::string sql = "DELETE FROM \"User\" WHERE user_id = " + std::to_string(userid) + ";";
        W.exec(sql);
        W.commit();
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Delete Error: " << e.what() << std::endl;
        return false;
    }
}

long UserDAO::createUser(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    try {
        pqxx::work W(*db->getConnection());
        std::string sql = "INSERT INTO \"User\" (username, password) VALUES (" + 
                          W.quote(username) + ", " + 
                          W.quote(password) + ") RETURNING user_id;";
        
        pqxx::row row = W.exec1(sql);
        W.commit();
        return row[0].as<long>();
    } catch (const std::exception &e) {
        std::cerr << "Register Error: " << e.what() << std::endl;
        return -1;
    }
}

std::optional<UserData> UserDAO::authenticate(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    try {
        pqxx::work W(*db->getConnection());
        std::string sql = "SELECT user_id, username, elo, COALESCE(info, '') "
                          "FROM \"User\" "
                          "WHERE username = " + W.quote(username) + 
                          " AND password = " + W.quote(password);
        
        pqxx::result R = W.exec(sql);
        
        if (R.size() == 1) {
            return UserData{
                R[0][0].as<long>(),
                R[0][1].as<std::string>(),
                R[0][2].as<int>(),  
                R[0][3].as<std::string>() 
            };
        }
        return std::nullopt;
    } catch (const std::exception &e) {
        std::cerr << "Login Error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool UserDAO::updateElo(long userId, int newElo) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    try {
        pqxx::work W(*db->getConnection());
        
        std::string sql = "UPDATE \"User\" SET elo = " + std::to_string(newElo) + 
                          " WHERE user_id = " + std::to_string(userId);
        
        W.exec(sql);
        W.commit();
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Update Elo Error: " << e.what() << std::endl;
        return false;
    }
}

bool UserDAO::updatePassword(long userId, const std::string& newPassword) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    try {
        pqxx::work W(*db->getConnection());
        
        std::string sql = "UPDATE \"User\" SET password = " + W.quote(newPassword) + 
                          " WHERE user_id = " + std::to_string(userId);
        
        W.exec(sql);
        W.commit();
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Update Password Error: " << e.what() << std::endl;
        return false;
    }
}

bool UserDAO::updateUsername(long userId, const std::string& newUsername) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    try {
        pqxx::work W(*db->getConnection());
        
        std::string sql = "UPDATE \"User\" SET username = " + W.quote(newUsername) + 
                            " WHERE user_id = " + std::to_string(userId);
        
        W.exec(sql);
        W.commit();
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Update Username Error: " << e.what() << std::endl;
        return false;
    }
}

std::optional<UserData> UserDAO::getUserById(long userId) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    try {
        pqxx::work W(*db->getConnection());
        std::string sql = "SELECT user_id, username, elo, COALESCE(password, '') FROM \"User\" WHERE user_id = " + std::to_string(userId);
        pqxx::result R = W.exec(sql);

        if (R.size() == 1) {
            return UserData{
                R[0][0].as<long>(),
                R[0][1].as<std::string>(),
                R[0][2].as<int>(),
                R[0][3].as<std::string>()
            };
        }
        return std::nullopt;
    } catch (const std::exception &e) {
        return std::nullopt;
    }
}

std::vector<UserData> UserDAO::getAllUsers() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<UserData> users;
    try {
        pqxx::work W(*db->getConnection());
        std::string sql = "SELECT user_id, username, elo, COALESCE(info, '') FROM \"User\" ORDER BY username ASC;";
        pqxx::result R = W.exec(sql);
        
        for (auto row : R) {
            users.push_back({
                row[0].as<long>(),
                row[1].as<std::string>(),
                row[2].as<int>(),
                row[3].as<std::string>() 
            });
        }
    } catch (const std::exception &e) {
        std::cerr << "GetAllUsers Error: " << e.what() << std::endl;
    }
    return users;
}
