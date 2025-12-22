#include "DatabaseServer.hpp"

DatabaseServer::DatabaseServer(const std::string& db_name, const std::string& user, const std::string& password) {
    connection_string = "dbname=" + db_name + " user=" + user + " password=" + password + " hostaddr=127.0.0.1 port=5432";
}

bool DatabaseServer::connect() {
    try {
        conn = std::make_unique<pqxx::connection>(connection_string);
        if (conn->is_open()) {
            std::cout << "Opened database successfully: " << conn->dbname() << std::endl;
            return true;
        } else {
            std::cout << "Can't open database" << std::endl;
            return false;
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
}

pqxx::connection* DatabaseServer::getConnection() {
    return conn.get();
}
