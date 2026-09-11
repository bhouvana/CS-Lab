// Plain assert-based tests, no framework.
#include <cassert>
#include <iostream>

#include "cache.hpp"
#include "parse_size.hpp"

static void test_first_access_is_always_a_miss_normal_case() {
    Cache cache({64, 16, 1}); // 4 lines, direct-mapped
    assert(cache.access(0x1000) == false);
    assert(cache.stats().misses == 1);
    std::cout << "ok: first access to any address is a miss\n";
}

static void test_repeated_access_is_a_hit_normal_case() {
    Cache cache({64, 16, 1});
    cache.access(0x1000);
    assert(cache.access(0x1000) == true); // same line, still resident
    assert(cache.stats().hits == 1);
    std::cout << "ok: repeated access to the same line hits\n";
}

static void test_same_line_different_offset_still_hits(void) {
    // 0x1000 and 0x1004 fall in the same 16-byte line.
    Cache cache({64, 16, 1});
    cache.access(0x1000);
    assert(cache.access(0x1004) == true);
    std::cout << "ok: different offsets within one line still hit\n";
}

static void test_direct_mapped_conflict_miss() {
    // 4 lines direct-mapped (64B/16B). 0x1000 and 0x1100 both map to
    // set index 0 (their line numbers differ by a multiple of
    // num_sets=4), so the second access evicts the first: a conflict miss.
    Cache cache({64, 16, 1});
    cache.access(0x1000);       // miss, fills set 0
    assert(cache.access(0x1100) == false); // conflict miss, evicts 0x1000's line
    assert(cache.access(0x1000) == false); // 0x1000 was evicted -> miss again
    std::cout << "ok: direct-mapped conflict miss evicts the colliding line\n";
}

static void test_set_associative_avoids_that_conflict() {
    // Same two conflicting addresses, but now 2-way associative: both
    // lines can coexist in the same set.
    Cache cache({64, 16, 2}); // 2 sets, 2 ways each
    cache.access(0x1000);
    cache.access(0x1100);
    assert(cache.access(0x1000) == true); // still resident thanks to associativity
    std::cout << "ok: 2-way associativity avoids the direct-mapped conflict\n";
}

static void test_lru_eviction_order() {
    // Fully associative, 2 lines. Access A, B, then A again (making B
    // the LRU), then C: C must evict B, not A.
    Cache cache({32, 16, 2}); // 1 set, 2 ways = fully associative
    cache.access(0x0);        // A
    cache.access(0x1000);     // B
    cache.access(0x0);        // A again -> A is now MRU, B is LRU
    cache.access(0x2000);     // C -> evicts B (the LRU line)
    assert(cache.access(0x0) == true);      // A survived
    assert(cache.access(0x1000) == false);  // B was evicted
    std::cout << "ok: LRU evicts the least-recently-used line, not the oldest inserted\n";
}

static void test_directive_example_trace() {
    // 0x1000, 0x1004, 0x1008, 0x2000, 0x1000 with a 16-byte line: the
    // first three addresses share one line (miss, hit, hit); 0x2000 is
    // a different line (miss); the final 0x1000 hits again.
    Cache cache({64, 16, 4});
    bool r1 = cache.access(0x1000);
    bool r2 = cache.access(0x1004);
    bool r3 = cache.access(0x1008);
    bool r4 = cache.access(0x2000);
    bool r5 = cache.access(0x1000);
    assert(r1 == false && r2 == true && r3 == true && r4 == false && r5 == true);
    assert(cache.stats().accesses == 5 && cache.stats().hits == 3 && cache.stats().misses == 2);
    std::cout << "ok: directive's example trace -> 3 hits, 2 misses\n";
}

static void test_invalid_config_rejected_invalid_case() {
    bool threw = false;
    try {
        Cache cache({100, 16, 4}); // 100 is not a multiple of 16*4=64
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok: non-divisible cache size rejected\n";
}

static void test_parse_size_suffixes() {
    assert(parse_size("4096") == 4096);
    assert(parse_size("32KB") == 32 * 1024ULL);
    assert(parse_size("1MB") == 1024ULL * 1024);
    assert(parse_size("64K") == 64 * 1024ULL);
    bool threw = false;
    try {
        parse_size("bogus");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok: parse_size handles B/KB/MB suffixes and rejects garbage\n";
}

int main() {
    test_first_access_is_always_a_miss_normal_case();
    test_repeated_access_is_a_hit_normal_case();
    test_same_line_different_offset_still_hits();
    test_direct_mapped_conflict_miss();
    test_set_associative_avoids_that_conflict();
    test_lru_eviction_order();
    test_directive_example_trace();
    test_invalid_config_rejected_invalid_case();
    test_parse_size_suffixes();
    std::cout << "all tests passed\n";
    return 0;
}
