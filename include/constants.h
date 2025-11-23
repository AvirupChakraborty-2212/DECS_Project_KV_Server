#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>

namespace Config {
    // Database Config
    const std::string DB_HOST = "tcp://127.0.0.1:3306";
    const std::string DB_USER = "mysql_user";
    const std::string DB_PASS = "abc@123"; // CHANGE THIS
    const std::string DB_NAME = "kv_store_db";

    // Server Config
    const std::string SERVER_ADDRESS = "127.0.0.1";
    const int SERVER_PORT = 8080;
    const int SERVER_THREAD_POOL_SIZE = 4; // Number of HTTP worker threads

    // Cache Config
    const int CACHE_CAPACITY_TOTAL = 1000; // Total items in cache
    const int CACHE_SHARDS = 4;            // Number of cache shards to reduce lock contention

    // DB Connection Pool Config
    const int DB_POOL_SIZE = 4; // Match thread pool size to avoid waiting
}

#endif // CONSTANTS_H