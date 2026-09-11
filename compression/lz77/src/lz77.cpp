// input -> sliding window search -> (offset, length, next) tokens
#include "lz77.hpp"

#include <algorithm>
#include <stdexcept>

std::vector<Token> lz77_compress(const std::vector<uint8_t>& data, Lz77Params params) {
    std::vector<Token> tokens;
    size_t n = data.size();
    size_t pos = 0;

    while (pos < n) {
        int best_len = 0;
        int best_offset = 0;

        size_t window_start = (pos > (size_t)params.window_size) ? pos - (size_t)params.window_size : 0;
        // Always reserve at least 1 byte for the trailing literal, so
        // `next` is never missing -- this is what avoids needing a
        // separate "match ran to end of input, no literal follows"
        // special case anywhere in encode or decode.
        size_t max_len = (n - pos > 1) ? std::min((size_t)params.max_match, n - pos - 1) : 0;

        for (size_t candidate = window_start; candidate < pos; candidate++) {
            size_t len = 0;
            // candidate + len can legally reach/exceed `pos` when the
            // match overlaps itself (e.g. a run of the same byte) --
            // that's intentional: data[candidate+len] for len that
            // makes candidate+len >= pos refers to bytes THIS match
            // itself is in the middle of producing conceptually, which
            // is exactly how "AAAA..." compresses to one long match.
            while (len < max_len && data[candidate + len] == data[pos + len]) len++;
            if (len > (size_t)best_len) {
                best_len = (int)len;
                best_offset = (int)(pos - candidate);
            }
        }

        Token t{};
        if (best_len > 0) {
            t.offset = (uint16_t)best_offset;
            t.length = (uint8_t)best_len;
            t.next = data[pos + best_len];
            pos += (size_t)best_len + 1;
        } else {
            t.offset = 0;
            t.length = 0;
            t.next = data[pos];
            pos += 1;
        }
        tokens.push_back(t);
    }
    return tokens;
}

std::vector<uint8_t> lz77_decompress(const std::vector<Token>& tokens) {
    std::vector<uint8_t> out;
    for (const Token& t : tokens) {
        if (t.length > 0) {
            size_t start = out.size() - t.offset;
            for (int i = 0; i < t.length; i++) out.push_back(out[start + (size_t)i]);
        }
        out.push_back(t.next);
    }
    return out;
}

std::vector<uint8_t> serialize_tokens(const std::vector<Token>& tokens) {
    std::vector<uint8_t> bytes;
    bytes.reserve(tokens.size() * 4);
    for (const Token& t : tokens) {
        bytes.push_back((uint8_t)(t.offset & 0xff));
        bytes.push_back((uint8_t)(t.offset >> 8));
        bytes.push_back(t.length);
        bytes.push_back(t.next);
    }
    return bytes;
}

std::vector<Token> deserialize_tokens(const std::vector<uint8_t>& bytes) {
    if (bytes.size() % 4 != 0) throw std::runtime_error("corrupt token stream: length not a multiple of 4");
    std::vector<Token> tokens;
    tokens.reserve(bytes.size() / 4);
    for (size_t i = 0; i < bytes.size(); i += 4) {
        Token t;
        t.offset = (uint16_t)(bytes[i] | (bytes[i + 1] << 8));
        t.length = bytes[i + 2];
        t.next = bytes[i + 3];
        tokens.push_back(t);
    }
    return tokens;
}
