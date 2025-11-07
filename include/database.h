#pragma once

#include <string>
#include <memory> // For std::unique_ptr

// MySQL Connector/C++ (header for includes)
#include <mysql_connection.h>
#include <mysql_driver.h>
#include <cppconn/exception.h>
#include <cppconn/driver.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

class DatabaseManager {
private:
    // Helper function to get a new DB connection
    static sql::Connection* getDbConnection();

public:
    // Functions for DB operations
    static void put(const std::string& key, const std::string& value);
    static std::string get(const std::string& key); // Returns empty string if not found
    static bool remove(const std::string& key);
};