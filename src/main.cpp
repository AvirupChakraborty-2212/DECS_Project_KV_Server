#include <iostream>
#include "httplib.h"
#include "constants.h"
#include "database.h"    // Ensure this matches your filename (e.g. db_pool.h or database.h)
#include "cache.h"  // Ensure this matches your filename (e.g. lru_cache.h or cache.h)

// Global singletons
DBPool* dbPool;
ShardedLRUCache* cache;

// Helper to execute SQL (Generic wrapper for simple inserts)
void exec_sql(const std::string& query, const std::string& k, const std::string& v = "") {
    sql::Connection* con = dbPool->getConnection();
    try {
        std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(query));
        pstmt->setString(1, k);
        if (!v.empty()) {
            pstmt->setString(2, v);
        }
        pstmt->executeUpdate();
    } catch (sql::SQLException &e) {
        std::cerr << "SQL Error: " << e.what() << std::endl;
    }
    dbPool->releaseConnection(con);
}

// --- Handlers ---

// 1. Create (POST /api/data?key=x&val=y)
void handle_create(const httplib::Request& req, httplib::Response& res) {
    if (req.has_param("key") && req.has_param("val")) {
        std::string k = req.get_param_value("key");
        std::string v = req.get_param_value("val");

        // DB Write (Insert or Update if exists)
        exec_sql("INSERT INTO key_value (key_name, value) VALUES (?, ?) ON DUPLICATE KEY UPDATE value = VALUES(value)", k, v);
        
        // Cache Write
        cache->put(k, v);

        res.set_content("Created", "text/plain");
    } else {
        res.status = 400;
    }
}

// 2. Read (GET /api/data?key=x)
void handle_read(const httplib::Request& req, httplib::Response& res) {
    if (req.has_param("key")) {
        std::string k = req.get_param_value("key");
        std::string v;

        // 1. Check Cache
        if (cache->get(k, v)) {
            // HIT: Set header for Load Generator to track
            res.set_header("X-Cache-Status", "HIT");
            res.set_content(v, "text/plain");
            return; 
        }

        // 2. Cache Miss - Fetch from DB
        sql::Connection* con = dbPool->getConnection();
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("SELECT value FROM key_value WHERE key_name = ?"));
            pstmt->setString(1, k);
            std::unique_ptr<sql::ResultSet> res_set(pstmt->executeQuery());

            if (res_set->next()) {
                v = res_set->getString("value");
                
                // Update Cache
                cache->put(k, v); 
                
                // MISS: Set header
                res.set_header("X-Cache-Status", "MISS");
                res.set_content(v, "text/plain");
            } else {
                res.status = 404;
                res.set_content("Not Found", "text/plain");
            }
        } catch (sql::SQLException &e) {
            std::cerr << "SQL Error in Read: " << e.what() << std::endl;
            res.status = 500;
        }
        dbPool->releaseConnection(con);
    } else {
        res.status = 400;
    }
}

// 3. Update (PUT /api/data?key=x&val=y)
void handle_update(const httplib::Request& req, httplib::Response& res) {
    if (req.has_param("key") && req.has_param("val")) {
        std::string k = req.get_param_value("key");
        std::string v = req.get_param_value("val");

        sql::Connection* con = dbPool->getConnection();
        int rows_affected = 0;

        try {
            // OPTIMIZATION: Directly try to update. 
            // executeUpdate() returns the number of rows matched/changed.
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("UPDATE key_value SET value = ? WHERE key_name = ?"));
            pstmt->setString(1, v);
            pstmt->setString(2, k);
            rows_affected = pstmt->executeUpdate();
        } catch (sql::SQLException &e) {
            std::cerr << "SQL Error in Update: " << e.what() << std::endl;
        }
        
        dbPool->releaseConnection(con);

        if (rows_affected > 0) {
            // If DB updated successfully, update cache
            cache->put(k, v);
            res.set_content("Updated", "text/plain");
        } else {
            // If 0 rows affected, key didn't exist
            res.status = 404;
            res.set_content("Key not found", "text/plain");
        }

    } else {
        res.status = 400;
    }
}

// 4. Delete (DELETE /api/data?key=x)
void handle_delete(const httplib::Request& req, httplib::Response& res) {
    if (req.has_param("key")) {
        std::string k = req.get_param_value("key");

        // DB Delete
        sql::Connection* con = dbPool->getConnection();
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("DELETE FROM key_value WHERE key_name = ?"));
            pstmt->setString(1, k);
            pstmt->executeUpdate();
        } catch (...) {}
        dbPool->releaseConnection(con);

        // Cache Delete
        cache->remove(k);

        res.set_content("Deleted", "text/plain");
    } else {
        res.status = 400;
    }
}

int main() {
    // Initialize singletons
    dbPool = new DBPool();
    cache = new ShardedLRUCache(Config::CACHE_CAPACITY_TOTAL, Config::CACHE_SHARDS);

    httplib::Server svr;
    
    // Configure thread pool
    svr.new_task_queue = [] { return new httplib::ThreadPool(Config::SERVER_THREAD_POOL_SIZE); };

    // Register Routes
    svr.Post("/api/data", handle_create);
    svr.Get("/api/data", handle_read);
    svr.Put("/api/data", handle_update);
    svr.Delete("/api/data", handle_delete);

        // --- ADD THIS DEBUGGING BLOCK HERE ---
    std::cout << "\n=== SERVER CONFIG DIAGNOSTICS ===" << std::endl;
    std::cout << "Server IP:        " << Config::SERVER_ADDRESS << std::endl;
    std::cout << "Server Port:      " << Config::SERVER_PORT << std::endl;
    std::cout << "Thread Pool Size: " << Config::SERVER_THREAD_POOL_SIZE << std::endl;
    std::cout << "Cache Capacity:   " << Config::CACHE_CAPACITY_TOTAL << std::endl;
    std::cout << "DB Pool Size:     " << Config::DB_POOL_SIZE << std::endl;
    std::cout << "=================================\n" << std::endl;
    // -------------------------------------

    std::cout << "Server started on port " << Config::SERVER_PORT << "..." << std::endl;
    svr.listen(Config::SERVER_ADDRESS.c_str(), Config::SERVER_PORT);

    // Cleanup (Only reached if server stops)
    delete cache;
    delete dbPool;
    return 0;
}