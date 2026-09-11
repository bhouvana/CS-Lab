#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <vector>

// One cache: direct-mapped (associativity=1), N-way set-associative, or
// fully-associative (associativity = size/line_size, i.e. one set) are
// all the same code path here — associativity is just a parameter.
// Replacement policy: LRU.
struct CacheConfig {
    size_t size_bytes;
    size_t line_size_bytes;
    int associativity;
};

struct CacheStats {
    long long accesses = 0;
    long long hits = 0;
    long long misses = 0;
    double hit_rate() const { return accesses > 0 ? (double)hits / accesses : 0.0; }
    double miss_rate() const { return accesses > 0 ? (double)misses / accesses : 0.0; }
};

class Cache {
public:
    explicit Cache(CacheConfig config);

    // Returns true on a hit, false on a miss. Always updates LRU order
    // and, on a miss, fills the line (evicting the LRU one if the set
    // is full).
    bool access(uint64_t address);

    const CacheStats& stats() const { return stats_; }
    size_t num_sets() const { return sets_.size(); }

private:
    size_t line_size_;
    int associativity_;
    // Each set: a list of tags, MRU at the front. A real cache slices
    // the address into bits; this simulator uses plain division/modulo
    // on the line number instead (see README limitations) — simpler,
    // and set count need not be a power of two.
    std::vector<std::list<uint64_t>> sets_;
    CacheStats stats_;
};
