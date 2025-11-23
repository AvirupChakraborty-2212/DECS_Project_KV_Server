#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <unordered_map>
#include <list>
#include <mutex>
#include <vector>
#include <functional>
#include <string>
#include "constants.h"

// A single partition of the cache
class LRUCacheShard {
private:
    size_t capacity;
    std::list<std::pair<std::string, std::string>> items;
    std::unordered_map<std::string, std::list<std::pair<std::string, std::string>>::iterator> cacheMap;
    std::mutex mtx;

public:
    LRUCacheShard(size_t cap) : capacity(cap) {}

    bool get(const std::string& key, std::string& value) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cacheMap.find(key);
        if (it == cacheMap.end()) {
            return false;
        }
        // Move to front (MRU)
        items.splice(items.begin(), items, it->second);
        value = it->second->second;
        return true;
    }

    void put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cacheMap.find(key);
        if (it != cacheMap.end()) {
            // Update existing
            it->second->second = value;
            items.splice(items.begin(), items, it->second);
        } else {
            // Insert new
            if (items.size() >= capacity) {
                auto last = items.end();
                last--;
                cacheMap.erase(last->first);
                items.pop_back();
            }
            items.push_front({key, value});
            cacheMap[key] = items.begin();
        }
    }

    void remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cacheMap.find(key);
        if (it != cacheMap.end()) {
            items.erase(it->second);
            cacheMap.erase(it);
        }
    }
};

// Wrapper to manage multiple shards
class ShardedLRUCache {
private:
    std::vector<LRUCacheShard*> shards;
    int num_shards;

    int getShardIndex(const std::string& key) {
        std::hash<std::string> hasher;
        return hasher(key) % num_shards;
    }

public:
    ShardedLRUCache(size_t total_capacity, int num_shards_in) : num_shards(num_shards_in) {
        size_t cap_per_shard = total_capacity / num_shards;
        if (cap_per_shard < 1) cap_per_shard = 1;
        for (int i = 0; i < num_shards; ++i) {
            shards.push_back(new LRUCacheShard(cap_per_shard));
        }
    }

    ~ShardedLRUCache() {
        for (auto s : shards) delete s;
    }

    bool get(const std::string& key, std::string& value) {
        return shards[getShardIndex(key)]->get(key, value);
    }

    void put(const std::string& key, const std::string& value) {
        shards[getShardIndex(key)]->put(key, value);
    }

    void remove(const std::string& key) {
        shards[getShardIndex(key)]->remove(key);
    }
};

#endif // LRU_CACHE_H