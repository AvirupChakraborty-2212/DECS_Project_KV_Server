#pragma once

#include <memory> // For std::shared_ptr
#include <string>

// spdlog for logging
#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h" // Not used in final implementation, but often useful for dev

// Forward declaration to avoid circular dependencies if Logger needs to log about other components
namespace httplib { class Server; }

// Global logger instance
extern std::shared_ptr<spdlog::logger> server_logger;

// Function to initialize the logger
void initialize_logger();

// Global flag to signal server to stop
extern std::atomic<bool> server_running;

// Forward declaration of httplib::Server (needed for global access in signal handler)
namespace httplib { class Server; }
extern httplib::Server* global_svr_ptr; // Pointer to the server instance

// Signal handler function
void signal_handler(int signum);