#pragma once

#include <string>
#include <cstddef> // For size_t

// Configuration constants for the database
const std::string DB_HOST = "tcp://127.0.0.1:3306";
const std::string DB_USER = "mysql_user";
const std::string DB_PASS = "abc@123";
const std::string DB_NAME = "kv_store_db";

// Configuration constants for the cache
const size_t CACHE_CAPACITY = 50; // Example capacity

// Server and client network configuration
const std::string SERVER_ADDRESS = "127.0.0.1";
const int SERVER_PORT = 8080;

// Log directory
const std::string LOG_DIR = "../logs";
