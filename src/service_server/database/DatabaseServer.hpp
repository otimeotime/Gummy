#ifndef DATABASE_SERVER_HPP
#define DATABASE_SERVER_HPP

#pragma once
#include <string>
#include <memory>
#include <iostream>
#include <pqxx/pqxx> 

class DatabaseServer {
private:
    std::unique_ptr<pqxx::connection> conn;
    std::string connection_string;

public:
    DatabaseServer(const std::string& db_name, const std::string& user, const std::string& password);

    bool connect();

    pqxx::connection* getConnection();
};

#endif // DATABASE_SERVER_HPP