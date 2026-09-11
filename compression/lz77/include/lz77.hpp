#pragma once

#include <cstdint>
#include <vector>

// LZ77: a sliding-window dictionary compressor. Each token is
// (offset, length, next_symbol): copy `length` bytes from `offset`
// bytes back in the already-produced output, then emit one literal
// byte `next`. offset == 0 means "no match" -- the token is just the
// literal `next` on its own.
struct Token {
    uint16_t offset; // 0 = no match
    uint8_t length;  // 0 if no match
    uint8_t next;    // one literal byte, always present
};

struct Lz77Params {
    int window_size = 4096; // how far back (bytes) a match may reference
    int max_match = 255;    // longest representable match (fits uint8_t)
};

std::vector<Token> lz77_compress(const std::vector<uint8_t>& data, Lz77Params params = {});
std::vector<uint8_t> lz77_decompress(const std::vector<Token>& tokens);

// Fixed 4-byte-per-token binary format (offset little-endian, then
// length, then next) -- real bytes on disk, not printed codes.
std::vector<uint8_t> serialize_tokens(const std::vector<Token>& tokens);
std::vector<Token> deserialize_tokens(const std::vector<uint8_t>& bytes);
