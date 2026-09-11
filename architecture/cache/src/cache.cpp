// memory address -> cache -> hit/miss
#include "cache.hpp"

#include <stdexcept>

Cache::Cache(CacheConfig config) : line_size_(config.line_size_bytes), associativity_(config.associativity) {
    if (config.size_bytes == 0 || config.line_size_bytes == 0 || config.associativity <= 0)
        throw std::invalid_argument("cache size, line size, and associativity must all be positive");

    size_t set_bytes = config.line_size_bytes * (size_t)config.associativity;
    if (config.size_bytes % set_bytes != 0)
        throw std::invalid_argument("cache size must be a multiple of line_size * associativity");

    size_t num_sets = config.size_bytes / set_bytes;
    sets_.resize(num_sets);
}

bool Cache::access(uint64_t address) {
    stats_.accesses++;

    uint64_t line_number = address / line_size_;
    size_t index = (size_t)(line_number % sets_.size());
    uint64_t tag = line_number / sets_.size();

    auto& set = sets_[index];
    for (auto it = set.begin(); it != set.end(); ++it) {
        if (*it == tag) {
            set.erase(it);
            set.push_front(tag); // move to MRU position
            stats_.hits++;
            return true;
        }
    }

    stats_.misses++;
    if ((int)set.size() >= associativity_) set.pop_back(); // evict LRU
    set.push_front(tag);
    return false;
}
