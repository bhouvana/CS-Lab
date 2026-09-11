// Experiments:
//   1. Compression ratio on repetitive vs. non-repetitive data.
//   2. Does a bigger window help once it already covers the data?
//   3. What would ideal variable-width token packing save?
//   4. How much faster is bounded hash-chain matching than naive search?
#include <fstream>
#include <chrono>
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

size_t packed_token_bytes(const std::vector<Token>& tokens) {
    size_t bits = 0;
    for (const Token& token : tokens) bits += token.offset == 0 ? 9 : 33;
    return (bits + 7) / 8;
}

void report_variable_encoding(const std::string& label, const std::vector<uint8_t>& data) {
    auto tokens = lz77_compress(data);
    auto fixed_bytes = serialize_tokens(tokens);
    auto packed_bytes = packed_token_bytes(tokens);
    double ratio = data.empty() ? 0.0 : 100.0 * (double)packed_bytes / (double)data.size();
    std::cout << std::left << std::setw(28) << label << std::setw(10) << data.size() << std::setw(10)
              << tokens.size() << std::setw(12) << fixed_bytes.size() << std::setw(12) << packed_bytes
              << std::fixed << std::setprecision(1) << ratio << "%\n";
}

std::vector<Token> lz77_compress_hash_chain(const std::vector<uint8_t>& data, Lz77Params params) {
    constexpr size_t hash_buckets = 1u << 16;
    std::vector<int> head(hash_buckets, -1);
    std::vector<int> previous(data.size(), -1);
    std::vector<Token> tokens;
    size_t indexed = 0;
    size_t pos = 0;

    auto hash_at = [&](size_t index) {
        return (((size_t)data[index] * 257u + data[index + 1]) * 257u + data[index + 2]) & (hash_buckets - 1);
    };
    auto index_until = [&](size_t limit) {
        while (indexed < limit && indexed + 2 < data.size()) {
            size_t bucket = hash_at(indexed);
            previous[indexed] = head[bucket];
            head[bucket] = (int)indexed;
            ++indexed;
        }
    };

    while (pos < data.size()) {
        index_until(pos);
        int best_len = 0;
        int best_offset = 0;
        size_t window_start = pos > (size_t)params.window_size ? pos - (size_t)params.window_size : 0;
        size_t max_len = data.size() - pos > 1 ? std::min((size_t)params.max_match, data.size() - pos - 1) : 0;
        if (pos + 2 < data.size()) {
            int candidate = head[hash_at(pos)];
            int examined = 0;
            while (candidate >= (int)window_start && candidate >= 0 && examined++ < 64) {
                size_t len = 0;
                while (len < max_len && data[(size_t)candidate + len] == data[pos + len]) ++len;
                if (len > (size_t)best_len) {
                    best_len = (int)len;
                    best_offset = (int)(pos - (size_t)candidate);
                }
                candidate = previous[(size_t)candidate];
            }
        }

        Token token{};
        if (best_len > 0) {
            token.offset = (uint16_t)best_offset;
            token.length = (uint8_t)best_len;
            token.next = data[pos + best_len];
            pos += (size_t)best_len + 1;
        } else {
            token.next = data[pos++];
        }
        tokens.push_back(token);
    }
    return tokens;
}

void report_speed(const std::string& label, const std::vector<uint8_t>& data, Lz77Params params) {
    using clock = std::chrono::steady_clock;
    constexpr auto minimum = std::chrono::milliseconds(50);
    size_t reps = 0;
    size_t total_tokens = 0;
    auto start = clock::now();
    do {
        auto tokens = lz77_compress(data, params);
        total_tokens += tokens.size();
        ++reps;
    } while (clock::now() - start < minimum);
    double elapsed_ms = std::chrono::duration<double, std::milli>(clock::now() - start).count();
    std::cout << std::left << std::setw(28) << label << std::setw(10) << data.size() << std::setw(10) << reps
              << std::fixed << std::setprecision(3) << elapsed_ms / (double)reps << " ms/run, "
              << (double)total_tokens / (elapsed_ms / 1000.0) << " tokens/sec\n";
}

void report_hash_speed(const std::string& label, const std::vector<uint8_t>& data, Lz77Params params) {
    using clock = std::chrono::steady_clock;
    constexpr auto minimum = std::chrono::milliseconds(50);
    size_t reps = 0;
    size_t total_tokens = 0;
    auto start = clock::now();
    do {
        auto tokens = lz77_compress_hash_chain(data, params);
        total_tokens += tokens.size();
        ++reps;
    } while (clock::now() - start < minimum);
    double elapsed_ms = std::chrono::duration<double, std::milli>(clock::now() - start).count();
    std::cout << std::left << std::setw(28) << label << std::setw(10) << data.size() << std::setw(10) << reps
              << std::fixed << std::setprecision(3) << elapsed_ms / (double)reps << " ms/run, "
              << (double)total_tokens / (elapsed_ms / 1000.0) << " tokens/sec\n";
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

    std::cout << "\nExperiment 3: variable-width token cost (ideal bit packing)\n\n";
    std::cout << std::left << std::setw(28) << "dataset" << std::setw(10) << "original" << std::setw(10)
              << "tokens" << std::setw(12) << "fixed bytes" << std::setw(12) << "packed bytes" << "ratio\n";
    report_variable_encoding("repetitive (A's + B's)", std::vector<uint8_t>(repetitive.begin(), repetitive.end()));
    if (!english.empty()) report_variable_encoding("English text", english);
    if (!source.empty()) report_variable_encoding("this lab's own source code", source);

    std::cout << "\nExperiment 4: naive match search speed as input grows\n\n";
    std::cout << std::left << std::setw(28) << "dataset" << std::setw(10) << "original" << std::setw(10)
              << "reps" << "average\n";
    std::vector<uint8_t> speed_data(16000, 'A');
    for (size_t i = 8; i < speed_data.size(); i += 16) speed_data[i] = 'B';
    report_speed("naive, 16,000-byte repetitive", speed_data, {});
    report_hash_speed("hash, 16,000-byte repetitive", speed_data, {});
    speed_data.resize(32000);
    for (size_t i = 16000; i < speed_data.size(); i++) speed_data[i] = (uint8_t)(i * 37);
    report_speed("naive, 32,000-byte mixed", speed_data, {});
    report_hash_speed("hash, 32,000-byte mixed", speed_data, {});

    return 0;
}
