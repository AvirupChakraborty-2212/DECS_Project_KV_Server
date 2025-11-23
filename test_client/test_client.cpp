#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <limits> // For std::numeric_limits

#include "httplib.h" // Assuming httplib.h is in your include path

#include "constants.h"

// Helper function to send HTTP requests and print results
std::string send_kv_request(const std::string& method, const std::string& key, const std::string& value = "") {
    httplib::Client cli(Config::SERVER_ADDRESS, Config::SERVER_PORT);
    cli.set_connection_timeout(0, 300000); // 300ms
    cli.set_read_timeout(5, 0);            // 5 seconds
    cli.set_write_timeout(5, 0);           // 5 seconds

    std::string path;
    httplib::Result res;
    httplib::Params params; // For POST requests with form parameters

    auto start_time = std::chrono::high_resolution_clock::now();

    if (method == "GET") {
        path = "/kv/" + key;
        res = cli.Get(path.c_str());
    } else if (method == "POST") { // Used for 'add' and 'update' in this client, as per server API
        path = "/kv"; // POST /kv expects form parameters
        params.emplace("key", key);
        params.emplace("value", value);
        res = cli.Post(path.c_str(), params);
    } else if (method == "PUT") {
        path = "/kv/" + key; 
        // httplib can send params in body for PUT similarly to POST
        params.emplace("value", value); 
        res = cli.Put(path.c_str(), params);
    }else if (method == "DELETE") {
        path = "/kv/" + key;
        res = cli.Delete(path.c_str());
    } else {
        std::cerr << "Error: Invalid internal HTTP method specified." << std::endl;
        return "Error: Invalid internal HTTP method specified.";
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double latency_ms = static_cast<double>(duration.count()) / 1000.0;

    std::cout << "Request Latency: " << std::fixed << std::setprecision(3) << latency_ms << " ms" << std::endl;

    if (res) {
        std::cout << "HTTP Status: " << res->status << std::endl;
        return res->body;
    } else {
        auto err = res.error();
        std::cerr << "Network/Client Error: " << httplib::to_string(err) << std::endl;
        return "Error: " + httplib::to_string(err);
    }
}

// Function to send a request for server statistics
std::string send_stats_request() {
    httplib::Client cli(Config::SERVER_ADDRESS, Config::SERVER_PORT);
    cli.set_connection_timeout(0, 300000); // 300ms
    cli.set_read_timeout(5, 0);            // 5 seconds
    cli.set_write_timeout(5, 0);           // 5 seconds

    auto start_time = std::chrono::high_resolution_clock::now();
    httplib::Result res = cli.Get("/stats");
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double latency_ms = static_cast<double>(duration.count()) / 1000.0;

    std::cout << "Request Latency: " << std::fixed << std::setprecision(3) << latency_ms << " ms" << std::endl;

    if (res) {
        std::cout << "HTTP Status: " << res->status << std::endl;
        return res->body;
    } else {
        auto err = res.error();
        std::cerr << "Network/Client Error: " << httplib::to_string(err) << std::endl;
        return "Error: " + httplib::to_string(err);
    }
}


int main() {
    std::cout << "Interactive KV Client" << std::endl;
    std::cout << "Server target: " << Config::SERVER_ADDRESS << ":" << Config::SERVER_PORT << std::endl;
    std::cout << "Type 'help' for commands." << std::endl;

    std::string command;
    std::string key, value;

    while (true) {
        std::cout << "\nEnter command (add, get, update, delete, stats, exit, help): ";
        std::cin >> command;

        // Clear the buffer after reading a single word (like "get" or "add")
        // This is crucial before subsequent std::getline calls
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (command == "exit") {
            break;
        } else if (command == "help") {
            std::cout << "\nAvailable commands:" << std::endl;
            std::cout << "  add      - Add a new key-value pair." << std::endl;
            std::cout << "  get      - Retrieve the value for a given key." << std::endl;
            std::cout << "  update   - Update the value for an existing key." << std::endl;
            std::cout << "  delete   - Remove a key-value pair." << std::endl;
            std::cout << "  stats    - Get server cache statistics." << std::endl;
            std::cout << "  exit     - Close the client." << std::endl;
            std::cout << std::endl;
        }
        else if (command == "get") {
            std::cout << "Enter key: ";
            std::getline(std::cin, key); // Use getline for keys as well, to allow spaces
            std::string response_body = send_kv_request("GET", key);
            std::cout << "Server Response Body:\n" << response_body << std::endl;

        } if (command == "add") {
            std::cout << "Enter key to add: ";
            std::getline(std::cin, key);
            std::cout << "Enter value: ";
            std::getline(std::cin, value);

            // "add" uses POST (Insert)
            // send_kv_request handles POST by sending to /kv with params
            std::string response = send_kv_request("POST", key, value);
            std::cout << "Response:\n" << response << std::endl;

        } else if (command == "update") {
            std::cout << "Enter key to update: ";
            std::getline(std::cin, key);
            std::cout << "Enter new value: ";
            std::getline(std::cin, value);

            // "update" uses PUT
            // We need to ensure send_kv_request handles PUT correctly
            std::string response = send_kv_request("PUT", key, value);
            std::cout << "Response:\n" << response << std::endl;

        } else if (command == "delete") {
            std::cout << "Enter key to delete: ";
            std::getline(std::cin, key);
            std::string response_body = send_kv_request("DELETE", key);
            std::cout << "Server Response Body:\n" << response_body << std::endl;

        } else if (command == "stats") {
            std::string response_body = send_stats_request();
            std::cout << "Server Response Body:\n" << response_body << std::endl;
            
        } else {
            std::cout << "Invalid command. Type 'help' for available commands." << std::endl;
        }
    }

    std::cout << "Exiting client." << std::endl;
    return 0;
}