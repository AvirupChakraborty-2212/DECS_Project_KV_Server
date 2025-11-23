## Project Title: 
**PERFORMANCE TESTING AND BENCHMARKING OF HTTP BASED KV SERVER**

## Architecture Diagram:
![Architecture](images/architecture.jpeg)


## Description: 
The goal of the project is to build a multi-tier system (HTTP server with a key-value (KV) storage system ), and perform its load test across various loads to identify its capacity and bottleneck resource.

**There are three main components in this system:** 
- a multi-threaded HTTP server with a KV cache (in-memory storage), 
- a multithreaded load generator (client side), and 
- a MySQL database (disk storage). 

The server is built over HTTP, and uses a pool of worker threads (httplib) to accept and process requests received from the clients. The clients generate requests to get and put key-value pairs at the server. The server stores all key-value pairs in a persistent MySQL database, and also caches the most frequently used key-value pairs in an in-memory LRU cache.The load generator will emulate multiple clients, and generate client requests concurrently to the server. 

**The implementation demonstrates handling of two types of requests that follow different execution paths like one accessing memory and another going to disk with the help of test client.** 

This is demonstrated by:

1.	Restart the server. This clears the in-memory cache.
2.	Before adding any keys through the client, we try to get a key that we know exists in your MySQL database from a previous run.
The first get after a server restart for key should result in source:"database".
```bash
Enter command (add, get, update, delete, stats, exit, help): get
Enter key: 2
Request Latency: 40.377 ms
HTTP Status: 200
Server Response Body:
{"key":"2","value":"3","source":"database"}
```
3.	Immediately get the same key again. This second get should then be source:"cache" because the first get (the miss) would have loaded it into the cache.
```bash
Enter command (add, get, update, delete, stats, exit, help): get
Enter key: 2
Request Latency: 4.646 ms
HTTP Status: 200
Server Response Body:
{"key":"2","value":"3","source":"cache"}
```

## Functionalities of the System Components:
1. **Server**: The server supports create, read, update and delete operations using RESTful APIs.
- **read**: When reading a key-value pair, first checks the cache. If it exists, reads it from the cache; otherwise, fetches it from the database and inserts it into the cache, evicting an existing pair if necessary.  
- **create**: When a new key-value pair is created, it is stored both in the cache and in the database. If the cache is full, evict an existing key-value pair based on LRU. 
- **update**: When a key is updated it is simultaneously updated in the database and the cache if the key exists.
svr.Post is used here instead of separate functions for Put and Update as it handles the insert and update operations in a compact manner within the same method (query).
- **delete**: Performs all delete operations on the database. If the affected key-value pair also exists in the cache, deletes it from the cache as well to synchronize it with the database and prevent inconsistent data.
- **stats**: using a new endpoint :  This returns the number of cache hits and cache misses and cache hit rate.

2. **Cache**: It is an in-memory sharded LRU cache. In the current server implementation, we are using the built-in C++ Standard Library to implement the LRU Cache.

3. **Database**: Connected a persistent KV store to the HTTP server, which stores data in the form of key-value pairs using MySQL to maintain the data sent by the clients using create, update, and delete operations. 
- **Read**: It checks whether a specific key is available in the database or not. If absent it throws an error.
- **Insert**: Here we are performing insert step based on whether a key is present or absent in the database.
- **Update**: It inserts a new key, value pair or else on duplicate key it updates the value.
- **Delete**: It deletes the dey if it exists or else throws an error. 

4. **DB connection Pool**:
Establishing a TCP connection to MySQL involves a handshake and authentication, which is computationally expensive. We implemented the Object Pool Pattern to mitigate this.
 - Structure: Athread-safe std::queue containing pre-established sql::Connection pointers.
 - Workflow:
 1. Borrow: A worker thread locks the pool mutex. If the queue is empty, it waits on a std::condition variable.
 2. Execute: The thread executes the SQL query.
 3. Return: The connection is pushed back into the queue, and waiting threads are notified.
 • Synchronization: We used std::condition variable to efficiently put threads to sleep if the pool is empty, waking them only when a connection is returned.

5. **Concurrency and thread safety**: 
We optimized the cache because a single lock is a bottleneck.
 - The Problem: In a multi-threaded environment, you need a std::mutex (Lock) to prevent two threads from corrupting the cache memory. If you have one big cache, all 4 threads fight for one lock. Thread A cannot read while Thread B is writing.
 - The Solution: Sharding– Partitioning: The cache is split into 4 independent shards.– Hashing Logic: The target shard is determined by hash arithmetic: Shard ID = Hash(Key) (mod 4)
 – Benefit: A thread accessing a key in Shard 0 does not block a thread accessing a key in Shard 1, significantly increasing parallel read/write throughput.

6. **Load Generator**: The Load Generator is designed as a high-performance, multi-threaded client application implemented in C++. It operates as a Closed-Loop System, where each thread waits for a response before issuing the next request. This model implies that the load generated is a function of the system’s response time (Little’s Law), providing a realistic simulation of active user behavior..

## Tech Stack: 
- Server is implemented in cpp. 
- Test Client is implemented in cpp.
- For server operations, httplib library is used. 
- Database (persistent storage): mysql server.
- Database connection libmysqlcppconn-dev is used.

## GitHub Repository Link: 
https://github.com/AvirupChakraborty-2212/DECS_Project_KV_Server

## Directory Structure:

    |-images
        |-architecture.jpeg
    |- include 
        |- cache.h
        |- constants.h
        |- database.h
        |- httplib.h
    |- src
        |- main.cpp
    |- loadgen
        |- load_generator.cpp
    |- CMakeLists.txt
    |- init_database.sql
    |- README.md
    |- run_load_gen.sh

**Note:** constants.h contains all the configurable parameters like the network configuration, cache capacity etc.

## Steps to setup and run the project(linux):


1. Install g++ and other essential libraries:

    ```bash
    sudo apt update
    sudo apt install build-essential
    sudo apt install -y wget curl git unzip cmake jq
    sudo apt install libmysqlcppconn-dev libcurl4-openssl-dev 
    ```

2. Install mysql-server:

    ```bash    
    sudo apt install mysql-server
    ```

3. Clone this github repository:

    ```bash
    git clone https://github.com/AvirupChakraborty-2212/DECS_Project_KV_Server.git
    ```

4. Setup mysql-server:

    ```bash    
    cd DECS_Project_KV_Server
    sudo mysql < create_db.sql -p
    sudo systemctl enable --now mysql
    ```

5. Build and Compile:

    ```bash
    mkdir build 
    cd build
    cmake ..
    make
    ```
    This will create the CMake files and the executables named `kv_server` and `test_client` in the `build/` directory.

6. Pin the database using taskset:

    ```bash
    sudo taskset -cp 0 $(pidof mysqld)
    ```
    You should see something like this 
    
    ```bash
    pid 394's current affinity list: 0-7
    pid 394's new affinity list: 0
    ```

7. Open new terminal window and navigate to the build directory and pin the server to some cores using taskset command:

    ```bash
    cd build
    taskset -c 1 ./kv_server
    ```
    You should see something like this in the terminal once the server is up and running:

    ```bash
    Server listening on port 8080...
    ```

8. Open one more terminal and change current working directory to build/:
   all workloads
    ```bash
    cd build    
    taskset -c 2-7 ./loadgen <no. of clients> <duration> <workload type>
    ```
    for mix workload
    ```bash
    cd build    
    taskset -c 2-7 ./loadgen <no. of clients> <duration> <workload type> <ratio1> <ratio2>
    ```

9. Verify taskset using the following example for the processes:
    ```bash
    taskset -c 1 ./kv_server
    pgrep kv_server
    taskset -p <PID_of_kv_server>
    ```

10. Incase you want to write the files to a csv use
    ```bash
    chmod +x run_load_gen.sh
    sudo ./run_load_gen.sh 10 300 put_all
    ```

## Sample output of the load generator:
```bash
(base) DECS/project_kv_server/build$ taskset -c 2-7 ./loadgen 5 300 put_all
>>> Starting Benchmark (put_all) with 5 threads for 300s...

=== RESULTS ===
Throughput: 423.43 req/sec
Latency: 11.58 ms
Cache: Hits=0 Misses=0 HitRate=0.00%
Disk: Writes=127028 404s=0
```