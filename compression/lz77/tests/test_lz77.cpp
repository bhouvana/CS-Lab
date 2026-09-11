// Plain assert-based tests, no framework.
#include <cassert>
#include <iostream>
#include <string>

#include "lz77.hpp"

static std::vector<uint8_t> to_bytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

static void roundtrip(const std::string& label, const std::vector<uint8_t>& data) {
    auto tokens = lz77_compress(data);
    auto restored = lz77_decompress(tokens);
    assert(restored == data);

    // Also round-trip through the binary serialization format.
    auto bytes = serialize_tokens(tokens);
    auto tokens2 = deserialize_tokens(bytes);
    auto restored2 = lz77_decompress(tokens2);
    assert(restored2 == data);

    std::cout << "ok: " << label << " (" << data.size() << " bytes -> " << tokens.size() << " tokens -> "
              << bytes.size() << " serialized bytes)\n";
}

static void test_directive_example_normal_case() {
    // AAAAAAAABAAAAAAA -- the exact example from the project directive.
    roundtrip("directive example (AAAAAAAABAAAAAAA)", to_bytes("AAAAAAAABAAAAAAA"));
}

static void test_english_text_normal_case() {
    roundtrip("English sentence", to_bytes("the quick brown fox jumps over the lazy dog"));
}

static void test_empty_input_edge_case() {
    roundtrip("empty input", {});
}

static void test_single_byte_edge_case() {
    roundtrip("single byte", to_bytes("X"));
}

static void test_two_identical_bytes_edge_case() {
    roundtrip("two identical bytes", to_bytes("AA"));
}

static void test_long_run_uses_overlapping_match() {
    // A run of 100 identical bytes should compress to very few tokens,
    // since an LZ77 match can legally overlap itself (offset=1).
    std::vector<uint8_t> data(100, 'Z');
    auto tokens = lz77_compress(data);
    assert(tokens.size() < 10); // a single overlapping match should cover nearly all of it
    auto restored = lz77_decompress(tokens);
    assert(restored == data);
    std::cout << "ok: a 100-byte run of one byte compresses to " << tokens.size() << " tokens\n";
}

static void test_all_256_byte_values_regression() {
    std::vector<uint8_t> data(256);
    for (int i = 0; i < 256; i++) data[i] = (uint8_t)i;
    roundtrip("all 256 byte values", data);
}

static void test_corrupt_token_stream_invalid_case() {
    std::vector<uint8_t> bad = {1, 2, 3}; // not a multiple of 4
    bool threw = false;
    try {
        deserialize_tokens(bad);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok: a corrupt (non-multiple-of-4) token stream is rejected\n";
}

static void test_window_size_limits_match_distance() {
    // With a tiny window, a match more than `window_size` bytes back
    // must NOT be found -- the repeated 'A' at the end is too far from
    // the 'A's at the start to be referenced.
    std::string s = "A" + std::string(20, 'x') + "A";
    Lz77Params tiny_window{5, 255};
    auto tokens = lz77_compress(to_bytes(s), tiny_window);
    for (const Token& t : tokens) assert(t.offset <= 5);
    auto restored = lz77_decompress(tokens);
    assert(restored == to_bytes(s));
    std::cout << "ok: window_size caps how far back a match may reference\n";
}

int main() {
    test_directive_example_normal_case();
    test_english_text_normal_case();
    test_empty_input_edge_case();
    test_single_byte_edge_case();
    test_two_identical_bytes_edge_case();
    test_long_run_uses_overlapping_match();
    test_all_256_byte_values_regression();
    test_corrupt_token_stream_invalid_case();
    test_window_size_limits_match_distance();
    std::cout << "all tests passed\n";
    return 0;
}
