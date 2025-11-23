cmake_minimum_required(VERSION 3.10)
project(CS744_KV_Project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ==============================
# Include directories (shared)
# ==============================
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    /usr/include/cppconn       # MySQL Connector/C++
)

# ==============================
# == SERVER (HTTP + MySQL)
# ==============================

# Gather all .cpp files in src/
file(GLOB SERVER_SOURCES src/*.cpp)

add_executable(kv_server ${SERVER_SOURCES})

# Find MySQL C++ Connector library
find_library(MYSQLCPP_CONN_LIB mysqlcppconn PATHS /usr/lib /usr/lib/x86_64-linux-gnu)

# Link libraries (MySQL + pthread)
target_link_libraries(kv_server PRIVATE ${MYSQLCPP_CONN_LIB} pthread)

# ==============================
# == LOAD GENERATOR (libcurl)
# ==============================

# Find libcurl package
find_package(CURL REQUIRED)

add_executable(loadgen loadgen/load_generator.cpp)

# Include and link curl
target_include_directories(loadgen PRIVATE ${CURL_INCLUDE_DIRS})
target_link_libraries(loadgen PRIVATE ${CURL_LIBRARIES} pthread)

# ==============================
# == TEST CLIENT (httplib)
# ==============================
# No special libraries needed beyond what's handled by include_directories for httplib.h
#add_executable(test_client test_client/test_client.cpp)
#target_include_directories(test_client PRIVATE ${CURL_INCLUDE_DIRS})
#target_link_libraries(test_client PRIVATE ${CURL_LIBRARIES} pthread) # For std::thread::sleep_for

# ==============================
# == Optional compiler warnings
# ==============================
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# ==============================
# == Summary Message
# ==============================
message(STATUS "-----------------------------------------")
message(STATUS "Project: CS744_KV_Project")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Server Source File: src/server.cpp")
message(STATUS "Load Generator Source File: loadgen/loadgen.cpp")
#message(STATUS "Test Client Source File: test_client.cpp")
message(STATUS "Include Dir (Project): ${CMAKE_SOURCE_DIR}/include")
message(STATUS "Include Dir (MySQL): /usr/include/mysql-cppconn-8/")
message(STATUS "MySQL C++ Connector Library: ${MYSQLCPP_CONN_LIB}")
message(STATUS "MySQL Client Library (for C++ Connector): ${MYSQLCLIENT_LIB}")
message(STATUS "Curl Include: ${CURL_INCLUDE_DIRS}")
message(STATUS "Curl Libraries: ${CURL_LIBRARIES}")
message(STATUS "-----------------------------------------")