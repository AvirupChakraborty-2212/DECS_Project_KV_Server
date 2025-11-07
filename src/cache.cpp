#include "cache.h"
#include "logger.h" // For server_logger

LruCache::LruCache(size_t capacity) : capacity(capacity) {}

bool LruCache::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(mtx); // Acquire lock for thread-safety
    auto it_map = cache_map.find(key);
    if (it_map == cache_map.end()) {
        return false; // Cache miss
    }
    // Cache hit: move the accessed item to the front of the list (MRU)
    cache_list.splice(cache_list.begin(), cache_list, it_map->second);
    value = it_map->second->value; // Update the value reference
    return true;
}

void LruCache::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mtx); // Acquire lock for thread-safety
    auto it_map = cache_map.find(key);

    if (it_map != cache_map.end()) {
        // Key exists: Update value and move to front (MRU)
        it_map->second->value = value;
        cache_list.splice(cache_list.begin(), cache_list, it_map->second);
    } else {
        // Key does not exist: Insert new item
        if (cache_map.size() >= capacity) {
            // Cache is full, evict LRU item (back of the list)
            std::string lru_key = cache_list.back().key;
            cache_map.erase(lru_key);
            cache_list.pop_back();
            if (server_logger) server_logger->trace("Cache eviction: Removed LRU key {}", lru_key);
        }
        // Insert new item at the front of the list (MRU)
        cache_list.push_front({key, value});
        cache_map[key] = cache_list.begin(); // Store iterator to list element
    }
    if (server_logger) server_logger->trace("Cache PUT: Key {}, Value {}", key, value);
}

bool LruCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx); // Acquire lock for thread-safety
    auto it_map = cache_map.find(key);
    if (it_map == cache_map.end()) {
        if (server_logger) server_logger->trace("Cache REMOVE: Key {} not found.", key);
        return false; // Key not in cache
    }
    // Remove from list and map
    cache_list.erase(it_map->second);
    cache_map.erase(it_map);
    if (server_logger) server_logger->trace("Cache REMOVE: Key {} removed.", key);
    return true;
}