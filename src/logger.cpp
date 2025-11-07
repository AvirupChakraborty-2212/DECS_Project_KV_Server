// logger.cpp

#include "logger.h"
#include "constants.h" // For LOG_DIR
#include "httplib.h"   // <--- ADD THIS LINE!

#include <iostream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <csignal>

// Initialize global variables
std::shared_ptr<spdlog::logger> server_logger;
std::atomic<bool> server_running{true};
httplib::Server* global_svr_ptr = nullptr;

void initialize_logger() {
    std::filesystem::create_directories(LOG_DIR);
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    std::string log_filename = LOG_DIR + "/server_log_" + ss.str() + ".txt";

    try {
        auto file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_filename, 0, 0);
        file_sink->set_level(spdlog::level::trace);

        server_logger = std::make_shared<spdlog::logger>("server_logger", spdlog::sinks_init_list({file_sink}));
        server_logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(server_logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        server_logger->info("Server logging initialized to {}", log_filename);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "CRITICAL: Server log initialization failed: " << ex.what() << std::endl;
        exit(1);
    }
}

void signal_handler(int signum) {
    if (server_logger) {
        server_logger->warn("Received signal {}, initiating graceful shutdown...", signum);
    }
    std::cout << "\nReceived signal " << signum << ", initiating graceful shutdown..." << std::endl;
    server_running.store(false);
    if (global_svr_ptr) {
        // Now the compiler has the full definition of httplib::Server from httplib.h
        global_svr_ptr->stop();
    }
}