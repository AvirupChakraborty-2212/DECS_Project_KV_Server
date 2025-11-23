#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <string>
#include <random>
#include <iomanip>
#include <algorithm>
#include "httplib.h"
#include "constants.h"

// --- CONFIGURATION ---
const int POPULAR_RANGE = 100;          // Keys 1-100 (Cache Hits)
const int LARGE_RANGE   = 100000;       // Keys 1-100,000 (Cache Misses / Disk Reads)
const int HUGE_RANGE    = 10000000;     // Keys 1-10,000,000 (Disk Writes)
const int MIXED_PREFILL = 2000;         // Keys per thread for Mixed History

// --- STATISTICS ---
std::atomic<long> total_requests(0);
std::atomic<long> successful_requests(0);
std::atomic<long> failed_requests(0);
std::atomic<long long> total_latency_ms(0);

// DETAILED METRICS
std::atomic<long> cache_hits(0);
std::atomic<long> cache_misses(0);
std::atomic<long> disk_writes(0);
std::atomic<long> disk_misses(0);

bool running = true;

enum WorkloadType { PUT_ALL, GET_ALL_UNIQUE, GET_POPULAR, MIXED };

// --- WARMUP PHASE ---
void perform_warmup(int id, int total_threads, WorkloadType type, std::string host, int port) {
    httplib::Client cli(host, port);
    cli.set_connection_timeout(30);
    cli.set_read_timeout(30);
    
    // 1. GET_POPULAR: Keys 1-100
    if (type == GET_POPULAR && id == 0) {
        std::cout << "[Warmup] Inserting " << POPULAR_RANGE << " popular keys...\n";
        for(int i=1; i<=POPULAR_RANGE; ++i) {
            cli.Post("/api/data", httplib::Params{{"key", std::to_string(i)}, {"val", "x"}});
        }
    }
    // 2. GET_ALL: Keys 1-100,000
    else if (type == GET_ALL_UNIQUE) {
        if (id == 0) std::cout << "[Warmup] Inserting " << LARGE_RANGE << " unique keys...\n";
        int per_thread = LARGE_RANGE / total_threads;
        int start = 1 + (id * per_thread);
        int end = start + per_thread;
        if (id == total_threads - 1) end = LARGE_RANGE + 1;

        for(int i=start; i<end; ++i) {
            cli.Post("/api/data", httplib::Params{{"key", std::to_string(i)}, {"val", "x"}});
        }
    }
    // 3. MIXED: Thread History
    else if (type == MIXED) {
        if (id == 0) std::cout << "[Warmup] Pre-filling " << MIXED_PREFILL << " keys per thread...\n";
        for(int i=1; i<=MIXED_PREFILL; ++i) {
            std::string k = std::to_string(id) + "_" + std::to_string(i);
            cli.Post("/api/data", httplib::Params{{"key", k}, {"val", "x"}});
        }
    }
    // PUT_ALL does not need Data Warmup (it writes new data), but the shell script runs it to warm up the connections.
}

// --- WORKER THREAD ---
void worker(int id, WorkloadType type, std::string host, int port, int p_get, int p_put) {
    httplib::Client cli(host, port);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(5);

    std::mt19937 rng(id + std::time(nullptr));
    std::uniform_int_distribution<int> dist_percent(0, 99);
    
    // Distributions for various workloads
    std::uniform_int_distribution<int> dist_popular(1, POPULAR_RANGE);
    std::uniform_int_distribution<int> dist_large(1, LARGE_RANGE);
    std::uniform_int_distribution<int> dist_huge(1, HUGE_RANGE);

    // Mixed Workload State
    long long local_max = MIXED_PREFILL; 

    while (running) {
        std::string key, val, path;
        auto start = std::chrono::high_resolution_clock::now();
        httplib::Result res;
        bool is_read = false, is_write = false;
        int p = dist_percent(rng);

      
        // 1. PUT ALL (Random Writes over Huge Range -> Forces Disk I/O)
        if (type == PUT_ALL) {
            // Use HUGE random range to prevent caching and force B-Tree splits
            key = std::to_string(dist_huge(rng));
            val = "val_" + key; // Payload
            
            if (p < p_get) { // Reusing param as Put%
                res = cli.Post("/api/data", httplib::Params{{"key", key}, {"val", val}});
                is_write = true;
            } else {
                // Delete random key (Disk intensive)
                path = "/api/data?key=" + key;
                res = cli.Delete(path.c_str());
                is_write = true;
            }
        }

        // 2. GET POPULAR (Cache Hits)
        else if (type == GET_POPULAR) {
            key = std::to_string(dist_popular(rng));
            path = "/api/data?key=" + key;
            res = cli.Get(path.c_str());
            is_read = true;
        }

        // 3. GET ALL UNIQUE (Cache Misses / Disk Reads)
        else if (type == GET_ALL_UNIQUE) {
            key = std::to_string(dist_large(rng));
            path = "/api/data?key=" + key;
            res = cli.Get(path.c_str());
            is_read = true;
        }

        // 4. MIXED (Sequential Growth)
        else if (type == MIXED) {
            if (p < p_get) { // GET
                std::uniform_int_distribution<long long> dist_hist(1, local_max);
                key = std::to_string(id) + "_" + std::to_string(dist_hist(rng));
                path = "/api/data?key=" + key;
                res = cli.Get(path.c_str());
                is_read = true;
            }
            else if (p < (p_get + p_put)) { // PUT
                local_max++;
                key = std::to_string(id) + "_" + std::to_string(local_max);
                val = "v_" + key;
                res = cli.Post("/api/data", httplib::Params{{"key", key}, {"val", val}});
                is_write = true;
            }
            else { // DELETE
                std::uniform_int_distribution<long long> dist_hist(1, local_max);
                key = std::to_string(id) + "_" + std::to_string(dist_hist(rng));
                path = "/api/data?key=" + key;
                res = cli.Delete(path.c_str());
                is_write = true;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        total_requests++;

        if (res) {
            if (res->status != 500) {
                successful_requests++;
                long long lat = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                total_latency_ms += lat;

                if (is_write) disk_writes++;
                if (is_read) {
                    if (res->has_header("X-Cache-Status")) {
                        if (res->get_header_value("X-Cache-Status") == "HIT") cache_hits++;
                        else cache_misses++;
                    } else {
                        if (res->status == 200) {
                            if (type == GET_POPULAR) cache_hits++; else cache_misses++;
                        }
                    }
                    if (res->status == 404) disk_misses++;
                }
            } else failed_requests++;
        } else failed_requests++;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Usage: ./loadgen <threads> <duration> <type> [p1] [p2] [--no-warmup]\n";
        return 1;
    }

    int threads = std::stoi(argv[1]);
    int seconds = std::stoi(argv[2]);
    std::string type_s = argv[3];
    WorkloadType type;
    int p_get = 0, p_put = 0;
    
    bool skip_warmup = false;
    for(int i=0; i<argc; ++i) {
        if(std::string(argv[i]) == "--no-warmup") skip_warmup = true;
    }

    if (type_s == "put_all") {
        type = PUT_ALL;
        if (argc > 4 && std::string(argv[4]) != "--no-warmup") p_get = std::stoi(argv[4]);
        else p_get = 100;
    }
    else if (type_s == "get_all") type = GET_ALL_UNIQUE;
    else if (type_s == "get_popular") type = GET_POPULAR;
    else if (type_s == "mix") {
        type = MIXED;
        if (argc > 4 && std::string(argv[4]) != "--no-warmup") p_get = std::stoi(argv[4]); else p_get = 80;
        if (argc > 5 && std::string(argv[5]) != "--no-warmup") p_put = std::stoi(argv[5]); else p_put = 10;
    } else {
        std::cerr << "Invalid type.\n"; return 1;
    }

    // AUTOMATIC WARMUP
    if (!skip_warmup && (type != PUT_ALL)) {
        std::cout << ">>> Warming up database...\n";
        std::vector<std::thread> w_threads;
        int w_count = (threads > 8) ? 8 : threads;
        for(int i=0; i<w_count; ++i) 
            w_threads.push_back(std::thread(perform_warmup, i, w_count, type, Config::SERVER_ADDRESS, Config::SERVER_PORT));
        for(auto& t : w_threads) t.join();
        std::cout << ">>> Warmup Complete.\n";
    }

    // BENCHMARK
    std::cout << ">>> Starting Benchmark (" << type_s << ") with " << threads << " threads for " << seconds << "s...\n";
    std::vector<std::thread> b_threads;
    for(int i=0; i<threads; ++i) 
        b_threads.push_back(std::thread(worker, i, type, Config::SERVER_ADDRESS, Config::SERVER_PORT, p_get, p_put));

    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    running = false;
    for(auto& t : b_threads) t.join();

    double tput = (double)successful_requests / seconds;
    double lat = (successful_requests > 0) ? (double)total_latency_ms / successful_requests : 0.0;
    long total_reads = cache_hits + cache_misses;
    double hit_rate = (total_reads > 0) ? ((double)cache_hits / total_reads * 100.0) : 0.0;

    std::cout << "\n=== RESULTS ===\n";
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) << tput << " req/sec\n";
    std::cout << "Latency: " << lat << " ms\n";
    std::cout << "Cache: Hits=" << cache_hits << " Misses=" << cache_misses << " HitRate=" << hit_rate << "%\n";
    std::cout << "Disk: Writes=" << disk_writes << " 404s=" << disk_misses << "\n";

    return 0;
}