// Experiments:
//   1. Does associativity fix conflict misses when several "hot" lines
//      alias to the same set?
//   2. Does increasing cache size improve hit rate once the working
//      set fits?
#include <iomanip>
#include <iostream>
#include <vector>

#include "cache.hpp"

namespace {
enum class Policy { LRU, FIFO, Random };

class PolicyCache {
public:
    PolicyCache(size_t size_bytes, size_t line_size, int associativity, Policy policy)
        : line_size_(line_size), associativity_(associativity), policy_(policy),
          sets_(size_bytes / (line_size * (size_t)associativity)) {}

    bool access(uint64_t address) {
        uint64_t line = address / line_size_;
        auto& set = sets_[(size_t)(line % sets_.size())];
        uint64_t tag = line / sets_.size();
        for (auto& entry : set) {
            if (entry.tag == tag) {
                if (policy_ == Policy::LRU) entry.last_used = ++clock_;
                return true;
            }
        }
        Entry incoming{tag, ++clock_, clock_};
        if ((int)set.size() < associativity_) {
            set.push_back(incoming);
        } else {
            size_t victim = 0;
            if (policy_ == Policy::Random) {
                random_state_ = random_state_ * 1664525u + 1013904223u;
                victim = random_state_ % set.size();
            } else if (policy_ == Policy::LRU) {
                for (size_t i = 1; i < set.size(); ++i)
                    if (set[i].last_used < set[victim].last_used) victim = i;
            } else {
                for (size_t i = 1; i < set.size(); ++i)
                    if (set[i].inserted < set[victim].inserted) victim = i;
            }
            set[victim] = incoming;
        }
        return false;
    }

private:
    struct Entry { uint64_t tag; uint64_t last_used; uint64_t inserted; };
    size_t line_size_;
    int associativity_;
    Policy policy_;
    std::vector<std::vector<Entry>> sets_;
    uint64_t clock_ = 0;
    uint32_t random_state_ = 0x9e3779b9u;
};

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

std::vector<uint64_t> make_workload_trace() {
    std::vector<uint64_t> trace;
    constexpr size_t matrix = 64;
    trace.reserve(matrix * matrix * 2);
    for (size_t row = 0; row < matrix; ++row)
        for (size_t col = 0; col < matrix; ++col) trace.push_back((row * matrix + col) * sizeof(int));
    for (size_t col = 0; col < matrix; ++col)
        for (size_t row = 0; row < matrix; ++row) trace.push_back((row * matrix + col) * sizeof(int));
    return trace;
}

double policy_hit_rate(const std::vector<uint64_t>& trace, Policy policy) {
    PolicyCache cache(4096, 64, 4, policy);
    size_t hits = 0;
    for (uint64_t address : trace) hits += cache.access(address) ? 1 : 0;
    return (double)hits / trace.size();
}

double two_level_amat(const std::vector<uint64_t>& trace, double* l1_rate, double* l2_rate) {
    Cache l1({1024, 64, 4});
    Cache l2({16384, 64, 8});
    for (uint64_t address : trace) {
        if (!l1.access(address)) l2.access(address);
    }
    *l1_rate = l1.stats().hit_rate();
    *l2_rate = l2.stats().hit_rate();
    double l1_misses = (double)l1.stats().misses;
    double l2_misses = (double)l2.stats().misses;
    return (l1.stats().hits * 1.0 + l1_misses * (10.0 + (l2_misses / l1_misses) * 100.0)) /
           (double)l1.stats().accesses;
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

    auto trace = make_workload_trace();
    std::cout << "\nExperiment 3: replacement policy on an instrumented row/column scan\n";
    std::cout << "(64x64 integer matrix, 8192 accesses, 4-way 4KB cache)\n\n";
    std::cout << std::left << std::setw(12) << "policy" << "hit rate\n";
    for (auto policy : {Policy::LRU, Policy::FIFO, Policy::Random}) {
        const char* label = policy == Policy::LRU ? "LRU" : policy == Policy::FIFO ? "FIFO" : "random";
        std::cout << std::left << std::setw(12) << label << (100.0 * policy_hit_rate(trace, policy)) << "%\n";
    }

    std::cout << "\nExperiment 4: two-level hierarchy effective average access time\n\n";
    double l1_rate, l2_rate;
    double amat = two_level_amat(trace, &l1_rate, &l2_rate);
    std::cout << "L1 hit rate: " << (100.0 * l1_rate) << "%\n";
    std::cout << "L2 hit rate (on L1 misses): " << (100.0 * l2_rate) << "%\n";
    std::cout << "AMAT (L1=1, L2=10, memory=100 cycles): " << amat << " cycles\n";
    return 0;
}
