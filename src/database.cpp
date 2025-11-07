#include "database.h"
#include "constants.h"
#include "logger.h" // For server_logger

sql::Connection* DatabaseManager::getDbConnection() {
    sql::mysql::MySQL_Driver *driver;
    sql::Connection *con;
    driver = sql::mysql::get_mysql_driver_instance();
    // Using a try-catch block to gracefully handle connection failures
    try {
        con = driver->connect(DB_HOST, DB_USER, DB_PASS);
        con->setSchema(DB_NAME);
        return con;
    } catch (sql::SQLException &e) {
        if (server_logger) server_logger->error("Database connection error: {}", e.what());
        throw; // Re-throw to be handled by calling functions
    }
}

void DatabaseManager::put(const std::string& key, const std::string& value) {
    std::unique_ptr<sql::Connection> con(getDbConnection()); // Unique connection per call
    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->prepareStatement("INSERT INTO key_value (key_name, value) VALUES (?, ?) ON DUPLICATE KEY UPDATE value = ?")
    );
    pstmt->setString(1, key);
    pstmt->setString(2, value);
    pstmt->setString(3, value);
    pstmt->execute();
    if (server_logger) server_logger->info("DB PUT: Key {}, Value {}", key, value);
}

std::string DatabaseManager::get(const std::string& key) {
    std::unique_ptr<sql::Connection> con(getDbConnection());
    std::unique_ptr<sql::PreparedStatement> pstmt(
        con->prepareStatement("SELECT value FROM key_value WHERE key_name = ?")
    );
    pstmt->setString(1, key);
    std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
    std::string value = "";
    if (res->next()) {
        value = res->getString("value");
        if (server_logger) server_logger->info("DB GET: Key {} found, Value {}", key, value);
        return value;
    }
    if (server_logger) server_logger->info("DB GET: Key {} not found.", key);
    return ""; // Not found
}

bool DatabaseManager::remove(const std::string& key) {
    try {
        std::unique_ptr<sql::Connection> con(getDbConnection());
        std::unique_ptr<sql::PreparedStatement> pstmt(
            con->prepareStatement("DELETE FROM key_value WHERE key_name = ?")
        );
        pstmt->setString(1, key);

        // executeUpdate() returns the number of affected rows
        int affected_rows = pstmt->executeUpdate();

        if (affected_rows > 0) {
            if (server_logger) server_logger->info("DB DELETE: Key {} was successfully removed. Affected rows: {}", key, affected_rows);
            return true; // Key was present and removed
        } else {
            if (server_logger) server_logger->info("DB DELETE: Key {} not found in database, no rows affected.", key);
            return false; // Key was not found, no rows removed
        }
    } catch (const sql::SQLException &e) {
        if (server_logger) server_logger->error("DB DELETE: Error deleting key {}: {}", key, e.what());
        throw; // Re-throw the exception to be handled by the caller (main server logic)
    }
}