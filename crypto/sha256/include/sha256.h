#ifndef SHA256_H
#define SHA256_H

// Educational implementation of SHA-256 (FIPS 180-4), from scratch — no
// OpenSSL, no libcrypto. Do not use this for anything that needs a
// vetted, hardened cryptographic library.

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];    // running hash value (a..h across blocks)
    uint64_t bit_len;      // total message length processed, in bits
    uint8_t buffer[64];    // partial 512-bit block not yet processed
    size_t buffer_len;     // bytes currently held in buffer (0..63)
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len);
// Applies padding, processes the final block(s), and writes the 32-byte
// digest. Do not call sha256_update() again after this.
void sha256_final(SHA256_CTX *ctx, uint8_t digest[32]);

// Convenience one-shot wrapper for data already fully in memory.
void sha256_hash(const uint8_t *data, size_t len, uint8_t digest[32]);

// Writes a 65-byte (64 hex chars + NUL) lowercase hex string.
void sha256_to_hex(const uint8_t digest[32], char hex[65]);

// SHA-224 (FIPS 180-4): the exact same compression function and message
// schedule as SHA-256, just a different initial hash value and a
// truncated (28-byte, not 32) output -- the difference is entirely in
// sha224_init/_final, not in the round logic itself. Same SHA256_CTX,
// same sha256_update() in between.
void sha224_init(SHA256_CTX *ctx);
void sha224_final(SHA256_CTX *ctx, uint8_t digest[28]);
void sha224_hash(const uint8_t *data, size_t len, uint8_t digest[28]);
void sha224_to_hex(const uint8_t digest[28], char hex[57]); // 56 hex chars + NUL

#endif
