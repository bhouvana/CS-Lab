// Experiments:
//   1. Compression ratio on repetitive vs. non-repetitive data.
//   2. Does a bigger window help once it already covers the data?
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "lz77.hpp"

namespace {
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void report(const std::string& label, const std::vector<uint8_t>& data, Lz77Params params) {
    auto tokens = lz77_compress(data, params);
    auto bytes = serialize_tokens(tokens);
    double ratio = data.empty() ? 0.0 : 100.0 * (double)bytes.size() / (double)data.size();
    std::cout << std::left << std::setw(28) << label << std::setw(10) << data.size() << std::setw(10)
               << tokens.size() << std::setw(12) << bytes.size() << std::fixed << std::setprecision(1) << ratio
               << "%\n";
}
} // namespace

int main() {
    std::cout << "Experiment 1: compression ratio by data type (default window=4096)\n\n";
    std::cout << std::left << std::setw(28) << "dataset" << std::setw(10) << "original" << std::setw(10)
               << "tokens" << std::setw(12) << "compressed" << "ratio\n";

    std::string repetitive(2000, 'A');
    for (size_t i = 8; i < repetitive.size(); i += 16) repetitive[i] = 'B'; // matches examples/repeat.txt's shape
    report("repetitive (A's + B's)", std::vector<uint8_t>(repetitive.begin(), repetitive.end()), {});

    auto english = read_file("examples/english.txt");
    if (!english.empty()) report("English text", english, {});

    auto source = read_file("src/lz77.cpp");
    if (!source.empty()) report("this lab's own source code", source, {});

    // A 200-byte block, repeated after 300 bytes of unrelated filler
    // (distance ~500) -- small enough that a window has to genuinely
    // reach back that far to find the repeat, unlike experiment 1's
    // pattern where every match was within 16 bytes regardless of
    // window size.
    std::string block, filler;
    for (int i = 0; i < 200; i++) block += (char)('a' + (i % 26));
    for (int i = 0; i < 300; i++) filler += (char)('A' + (i % 26));
    std::string far_apart = block + filler + block;
    std::vector<uint8_t> far_apart_bytes(far_apart.begin(), far_apart.end());

    std::cout << "\nExperiment 2: a match ~500 bytes back -- does window size reaching\n";
    std::cout << "that far actually matter?\n\n";
    std::cout << std::left << std::setw(14) << "window" << std::setw(10) << "tokens" << "compressed\n";
    for (int window : {16, 64, 256, 512, 1024, 4096}) {
        Lz77Params p{window, 255};
        auto tokens = lz77_compress(far_apart_bytes, p);
        auto bytes = serialize_tokens(tokens);
        std::cout << std::left << std::setw(14) << window << std::setw(10) << tokens.size() << bytes.size()
                   << "\n";
    }
    return 0;
}
