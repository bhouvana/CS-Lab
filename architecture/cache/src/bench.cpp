// Experiments:
//   1. Does associativity fix conflict misses when several "hot" lines
//      alias to the same set?
//   2. Does increasing cache size improve hit rate once the working
//      set fits?
#include <iomanip>
#include <iostream>

#include "cache.hpp"

namespace {
// K streams, each a single line, placed exactly `stride` bytes apart.
// Round-robin touching all K, R times, means: if the cache can hold
// fewer than K of them at once, every stream's line is evicted long
// before its next visit (zero reuse); if it can hold all K, every
// access after the first round is a hit. This is a genuine, sharp
// cliff, not a smooth curve — see the README for why that's the
// honest, correct result for this access pattern.
double run_aliasing_experiment(size_t cache_size, size_t line_size, int assoc, int num_streams, int rounds) {
    Cache cache({cache_size, line_size, assoc});
    for (int r = 0; r < rounds; r++)
        for (int s = 0; s < num_streams; s++) cache.access((uint64_t)s * cache_size);
    return cache.stats().hit_rate();
}

// Sequentially scans a `working_set_lines`-line region, `rounds` times.
// Same cliff logic: if the cache can't hold the whole working set, a
// sequential scan gives zero reuse between rounds.
double run_working_set_experiment(size_t cache_size, size_t line_size, int assoc, int working_set_lines,
                                   int rounds) {
    Cache cache({cache_size, line_size, assoc});
    for (int r = 0; r < rounds; r++)
        for (int line = 0; line < working_set_lines; line++) cache.access((uint64_t)line * line_size);
    return cache.stats().hit_rate();
}
} // namespace

int main() {
    const size_t line_size = 64;
    const size_t cache_size = 4096; // 64 lines total, at every associativity
    const int num_streams = 8;
    const int rounds = 200;

    std::cout << "Experiment 1: associativity vs. aliasing conflict misses\n";
    std::cout << "(" << num_streams << " hot lines all aliasing to the same set, " << cache_size
              << "-byte cache, " << rounds << " rounds)\n\n";
    std::cout << std::left << std::setw(12) << "ways" << "hit rate\n";
    std::cout << std::fixed << std::setprecision(2);
    for (int ways : {1, 2, 4, 8}) {
        double hr = run_aliasing_experiment(cache_size, line_size, ways, num_streams, rounds);
        std::cout << std::left << std::setw(12) << ways << (100.0 * hr) << "%\n";
    }

    std::cout << "\nExperiment 2: cache size vs. hit rate for a fixed working set\n";
    std::cout << "(64-line / 4096-byte working set, 4-way, " << rounds << " rounds)\n\n";
    std::cout << std::left << std::setw(12) << "size" << "hit rate\n";
    struct SizeLabel { size_t bytes; const char* label; };
    for (auto sl : {SizeLabel{1024, "1KB"}, SizeLabel{2048, "2KB"}, SizeLabel{4096, "4KB"}, SizeLabel{8192, "8KB"}}) {
        double hr = run_working_set_experiment(sl.bytes, line_size, 4, 64, rounds);
        std::cout << std::left << std::setw(12) << sl.label << (100.0 * hr) << "%\n";
    }
    return 0;
}
