#ifndef DB_POOL_H
#define DB_POOL_H

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/exception.h>

#include <queue>
#include <mutex>
#include <condition_variable>
#include "constants.h"

class DBPool {
private:
    std::queue<sql::Connection*> connections;
    std::mutex mtx;
    std::condition_variable cv;
    sql::mysql::MySQL_Driver* driver;

public:
    DBPool() {
        driver = sql::mysql::get_mysql_driver_instance();
        for (int i = 0; i < Config::DB_POOL_SIZE; ++i) {
            try {
                sql::Connection* con = driver->connect(Config::DB_HOST, Config::DB_USER, Config::DB_PASS);
                con->setSchema(Config::DB_NAME);
                connections.push(con);
            } catch (sql::SQLException &e) {
                fprintf(stderr, "Error connecting to DB: %s\n", e.what());
            }
        }
    }

    ~DBPool() {
        std::lock_guard<std::mutex> lock(mtx);
        while (!connections.empty()) {
            delete connections.front();
            connections.pop();
        }
    }

    sql::Connection* getConnection() {
        std::unique_lock<std::mutex> lock(mtx);
        while (connections.empty()) {
            cv.wait(lock);
        }
        sql::Connection* con = connections.front();
        connections.pop();
        return con;
    }

    void releaseConnection(sql::Connection* con) {
        std::unique_lock<std::mutex> lock(mtx);
        connections.push(con);
        cv.notify_one();
    }
};

#endif // DB_POOL_H