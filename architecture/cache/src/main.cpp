// CLI: cache --size 32KB --line 64 --assoc 4 trace.txt
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "cache.hpp"
#include "parse_size.hpp"

namespace {
int usage(const char* prog) {
    std::cerr << "usage: " << prog << " --size <bytes|32KB|1MB> --line <bytes> --assoc <ways> <trace-file>\n";
    return 2;
}

// Trace file: one address per line, hex ("0x1000") or decimal.
std::vector<uint64_t> load_trace(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<uint64_t> addresses;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        addresses.push_back(std::stoull(line, nullptr, 0)); // base 0: auto-detects "0x"
    }
    return addresses;
}
} // namespace

int main(int argc, char** argv) {
    size_t size_bytes = 0, line_bytes = 0;
    int assoc = 0;
    std::string trace_path;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) size_bytes = parse_size(argv[++i]);
        else if (std::strcmp(argv[i], "--line") == 0 && i + 1 < argc) line_bytes = parse_size(argv[++i]);
        else if (std::strcmp(argv[i], "--assoc") == 0 && i + 1 < argc) assoc = std::atoi(argv[++i]);
        else trace_path = argv[i];
    }

    if (size_bytes == 0 || line_bytes == 0 || assoc == 0 || trace_path.empty()) return usage(argv[0]);

    Cache cache({size_bytes, line_bytes, assoc});
    std::vector<uint64_t> trace;
    try {
        trace = load_trace(trace_path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    for (uint64_t addr : trace) cache.access(addr);

    const CacheStats& s = cache.stats();
    std::cout << "Cache Configuration\n-------------------\n";
    std::cout << "Size:          " << size_bytes << " bytes\n";
    std::cout << "Line size:     " << line_bytes << " bytes\n";
    std::cout << "Associativity: " << assoc << "-way\n";
    std::cout << "Sets:          " << cache.num_sets() << "\n\n";
    std::cout << "Results\n-------\n";
    std::cout << "Accesses:  " << s.accesses << "\n";
    std::cout << "Hits:      " << s.hits << "\n";
    std::cout << "Misses:    " << s.misses << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Hit rate:  " << (100.0 * s.hit_rate()) << "%\n";
    std::cout << "Miss rate: " << (100.0 * s.miss_rate()) << "%\n";
    return 0;
}
