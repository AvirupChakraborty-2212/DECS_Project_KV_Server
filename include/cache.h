#pragma once

#include <string>
#include <unordered_map>
#include <list>
#include <mutex> // For std::mutex
#include <cstddef> // For size_t

// Define CacheEntry struct
struct CacheEntry {
    std::string key;
    std::string value;
};

// LruCache class definition
class LruCache {
private:
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> cache_map;
    std::list<CacheEntry> cache_list; // Front is MRU, Back is LRU
    size_t capacity;
    mutable std::mutex mtx; // Protects map and list access

public:
    LruCache(size_t capacity);

    bool get(const std::string& key, std::string& value);
    void put(const std::string& key, const std::string& value);
    bool remove(const std::string& key);
    size_t size() const { std::lock_guard<std::mutex> lock(mtx); return cache_map.size(); }
    void clear() { std::lock_guard<std::mutex> lock(mtx); cache_map.clear(); cache_list.clear(); }
};