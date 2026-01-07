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

std::optional<ProfileHeaderData> UserDAO::getProfileHeaderByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    try {
        pqxx::work W(*db->getConnection());
        const auto R = W.exec_params(
            "SELECT user_id, username, COALESCE(info, ''), created_at, elo FROM \"User\" WHERE username = $1",
            username);
        if (R.size() != 1) return std::nullopt;

        ProfileHeaderData out;
        out.id = R[0][0].as<long>(0);
        out.username = R[0][1].as<std::string>("");
        out.info = R[0][2].as<std::string>("");
        out.createdAt = R[0][3].as<std::string>("");
        out.elo = R[0][4].as<int>(0);
        return out;
    } catch (const std::exception& e) {
        std::cerr << "GetProfileHeader Error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<RecentMatchData> UserDAO::getRecentMatchesForUser(long userId, int limit) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    std::vector<RecentMatchData> out;
    try {
        pqxx::work W(*db->getConnection());
        const int lim = (limit <= 0) ? 20 : std::min(limit, 20);

        // Fetch most recent matches where this user is a player.
        // We also pull opponent username + both players' scores (if present).
        const auto R = W.exec_params(
            "SELECT "
            "  m.match_id, "
            "  COALESCE(to_char(m.ended_at, 'YYYY-MM-DD HH24:MI:SS'), ''), "
            "  COALESCE(opp_u.username, ''), "
            "  COALESCE(self_p.score, 0), "
            "  COALESCE(opp_p.score, 0), "
            "  COALESCE(m.is_draw, FALSE), "
            "  COALESCE(m.winner_user_id, 0), "
            "  COALESCE(m.log_path, '') "
            "FROM \"MatchParticipant\" self_p "
            "JOIN \"Match\" m ON m.match_id = self_p.match_id "
            "LEFT JOIN \"MatchParticipant\" opp_p "
            "  ON opp_p.match_id = self_p.match_id "
            "  AND opp_p.user_id <> self_p.user_id "
            "  AND opp_p.is_player = TRUE "
            "LEFT JOIN \"User\" opp_u ON opp_u.user_id = opp_p.user_id "
            "WHERE self_p.user_id = $1 AND self_p.is_player = TRUE "
            "ORDER BY m.started_at DESC "
            "LIMIT $2",
            userId,
            lim);

        out.reserve((size_t)R.size());
        for (const auto& row : R) {
            RecentMatchData m;
            m.matchId = row[0].as<long long>(0);
            m.endedAt = row[1].as<std::string>("");
            m.opponent = row[2].as<std::string>("");
            m.myScore = row[3].as<int>(0);
            m.oppScore = row[4].as<int>(0);
            m.isDraw = row[5].as<bool>(false);
            m.winnerUserId = (uint32_t)row[6].as<long long>(0);
            m.logPath = row[7].as<std::string>("");
            out.push_back(std::move(m));
        }
    } catch (const std::exception& e) {
        std::cerr << "GetRecentMatches Error: " << e.what() << std::endl;
    }
    return out;
}
