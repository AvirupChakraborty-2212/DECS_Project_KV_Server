// main.cpp

// --- Includes from separated components ---
#include "constants.h"
#include "logger.h"
#include "database.h"
#include "cache.h"

// httplib (header-only)
#include "httplib.h"

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <list>
#include <stdexcept>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>
#include <csignal> // For std::signal


// --- Main HTTP Server Logic ---
int main() {
    // Setup logging
    initialize_logger();

    // Register signal handler for SIGINT (Ctrl+C) and SIGTERM
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Initialize global atomic counters
    std::atomic<long long> cache_hits{0};
    std::atomic<long long> cache_misses{0};

    // Initialize LruCache instance
    LruCache cache(CACHE_CAPACITY);
    server_logger->info("LRU Cache initialized with capacity: {}", CACHE_CAPACITY);

    // Initialize httplib server
    httplib::Server svr;

    global_svr_ptr = &svr; // Set global pointer for signal handler

    // --- Log Suggested Thread Count (httplib manages its own pool by default) ---
    unsigned int num_cpu_cores = std::thread::hardware_concurrency();
    unsigned int suggested_worker_threads = 0;

    if (num_cpu_cores > 0) {
        // Log a suggested number, but httplib's internal mechanism will decide
        suggested_worker_threads = num_cpu_cores > 1 ? num_cpu_cores - 1 : 1;
    } else {
        suggested_worker_threads = 4; // A reasonable default if hardware_concurrency fails
        server_logger->warn("std::thread::hardware_concurrency() returned 0. Suggesting {} worker threads.", suggested_worker_threads);
    }

    // httplib typically uses its own internal thread pool which may default to num_cpu_cores
    // or scale dynamically. We are just logging a suggestion here.
    server_logger->info("System hardware concurrency: {}. Suggested worker threads: {}.",
                         num_cpu_cores, suggested_worker_threads);
    // ------------------------------------

    // Define REST API endpoints
    // PUT /kv (Create/Update)
    svr.Post("/kv", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.get_param_value("key");
        std::string value = req.get_param_value("value");

        if (key.empty() || value.empty()) {
            res.status = 400; // Bad Request
            res.set_content("{\"error\":\"key and value are required\"}", "application/json");
            server_logger->warn("PUT /kv: Bad Request - Missing key or value.");
            return;
        }

        try {
            DatabaseManager::put(key, value); // Write to DB first
            cache.put(key, value);             // Then update cache
            res.status = 200;
            res.set_content("{\"status\":\"success\"}", "application/json");
            server_logger->info("PUT /kv: Key {} processed.", key);
        } catch (const sql::SQLException& e) {
            server_logger->error("PUT /kv: DB Error for key {}: {}", key, e.what());
            res.status = 500;
            res.set_content("{\"error\":\"database error\"}", "application/json");
        } catch (const std::exception& e) {
            server_logger->error("PUT /kv: General error for key {}: {}", key, e.what());
            res.status = 500;
            res.set_content("{\"error\":\"server error\"}", "application/json");
        }
    });

    // GET /kv/:key (Read)
    svr.Get(R"(/kv/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1]; // Get key from URL regex

        std::string value;
        if (cache.get(key, value)) {
            // --- Execution Path 2: Cache Hit (Memory Access) ---
            cache_hits.fetch_add(1, std::memory_order_relaxed);
            res.status = 200;
            res.set_content("{\"key\":\"" + key + "\",\"value\":\"" + value + "\",\"source\":\"cache\"}", "application/json");
            server_logger->info("GET /kv/{}: Cache HIT.", key);
            return; // Request satisfied from memory
        }

        // --- Execution Path 1: Cache Miss (Disk/Database Access) ---
        cache_misses.fetch_add(1, std::memory_order_relaxed);
        try {
            value = DatabaseManager::get(key); // Go to database (disk)
            if (!value.empty()) {
                cache.put(key, value); // Add to cache after fetching from DB
                res.status = 200;
                res.set_content("{\"key\":\"" + key + "\",\"value\":\"" + value + "\",\"source\":\"database\"}", "application/json");
                server_logger->info("GET /kv/{}: Cache MISS, found in DB.", key);
            } else {
                res.status = 404; // Not Found
                res.set_content("{\"error\":\"key not found\"}", "application/json");
                server_logger->info("GET /kv/{}: Not Found in DB.", key);
            }
        } catch (const sql::SQLException& e) {
            server_logger->error("GET /kv/{}: DB Error: {}", key, e.what());
            res.status = 500;
            res.set_content("{\"error\":\"database error\"}", "application/json");
        } catch (const std::exception& e) {
            server_logger->error("GET /kv/{}: General error: {}", key, e.what());
            res.status = 500;
            res.set_content("{\"error\":\"server error\"}", "application/json");
        }
    });

    // DELETE /kv/:key (Delete)
    svr.Delete(R"(/kv/(.*))", [&](const httplib::Request& req, httplib::Response& res) {
        std::string key = req.matches[1];

        try {
            bool deleted_from_db = DatabaseManager::remove(key); // Get the result of the deletion
            cache.remove(key); // Always try to remove from cache, regardless of DB presence

            if (deleted_from_db) {
                res.status = 200;
                res.set_content("{\"status\":\"success\",\"message\":\"key deleted\"}", "application/json");
                server_logger->info("DELETE /kv/{}: Key successfully deleted.", key);
            } else {
                // Key was not found in the DB
                res.status = 404; // Not Found
                res.set_content("{\"error\":\"key not found\"}", "application/json");
                server_logger->info("DELETE /kv/{}: Key not found in DB, nothing to delete.", key);
            }
        } catch (const sql::SQLException& e) {
            server_logger->error("DELETE /kv/{}: DB Error: {}", key, e.what());
            res.status = 500;
            res.set_content("{\"error\":\"database error\"}", "application/json");
        } catch (const std::exception& e) {
            server_logger->error("DELETE /kv/{}: General error: {}", key, e.what());
            res.status = 500;
            res.set_content("{\"error\":\"server error\"}", "application/json");
        }
    });

    // NEW ENDPOINT: GET /stats
    svr.Get("/stats", [&](const httplib::Request&, httplib::Response& res) {
        long long current_hits = cache_hits.load();
        long long current_misses = cache_misses.load();
        long long total_get_requests = current_hits + current_misses;
        double hit_rate = 0.0;

        if (total_get_requests > 0) {
            hit_rate = static_cast<double>(current_hits) / total_get_requests * 100.0;
        }

        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << hit_rate;
        std::string hit_rate_str = ss.str();

        std::string json_response = "{"
                                    "\"cache_hits\":" + std::to_string(current_hits) + ","
                                    "\"cache_misses\":" + std::to_string(current_misses) + ","
                                    "\"total_get_requests\":" + std::to_string(total_get_requests) + ","
                                    "\"cache_hit_rate\":\"" + hit_rate_str + "%\""
                                    "}";
        res.status = 200;
        res.set_content(json_response, "application/json");
        server_logger->info("GET /stats: Cache stats requested.");
    });

    // Start server
    std::cout << "Server listening on port " << SERVER_PORT << "..." << std::endl;
    server_logger->info("Server listening on port {}...", SERVER_PORT);
    svr.listen(SERVER_ADDRESS.c_str(), SERVER_PORT);
    server_logger->info("Server shutting down gracefully.");

    global_svr_ptr = nullptr; // Clear global pointer after server stops

    return 0;
}