// SHA-256 pipeline:
//
//   message -> padding -> 512-bit blocks -> message schedule
//           -> 64 rounds -> hash state -> 256-bit digest
//
// Implemented directly from FIPS 180-4, no external crypto library.
#include "sha256.h"

#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static const uint32_t H0[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

static uint32_t rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

// Choice: for each bit, pick from y if x's bit is 1, else from z.
static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

// Majority: for each bit, the value at least two of x/y/z agree on.
static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t big_sigma0(uint32_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

static uint32_t big_sigma1(uint32_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

static uint32_t small_sigma0(uint32_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

static uint32_t small_sigma1(uint32_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

// Message schedule + 64 compression rounds over one 512-bit block.
static void process_block(SHA256_CTX *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    for (int t = 0; t < 16; t++) {
        w[t] = ((uint32_t)block[t * 4] << 24) | ((uint32_t)block[t * 4 + 1] << 16) |
               ((uint32_t)block[t * 4 + 2] << 8) | (uint32_t)block[t * 4 + 3];
    }
    for (int t = 16; t < 64; t++) {
        w[t] = small_sigma1(w[t - 2]) + w[t - 7] + small_sigma0(w[t - 15]) + w[t - 16];
    }

    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5], g = ctx->state[6], h = ctx->state[7];

    for (int t = 0; t < 64; t++) {
        uint32_t t1 = h + big_sigma1(e) + ch(e, f, g) + K[t] + w[t];
        uint32_t t2 = big_sigma0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void sha256_init(SHA256_CTX *ctx) {
    memcpy(ctx->state, H0, sizeof(H0));
    ctx->bit_len = 0;
    ctx->buffer_len = 0;
}

void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len) {
    ctx->bit_len += (uint64_t)len * 8;
    size_t i = 0;

    if (ctx->buffer_len > 0) {
        size_t need = 64 - ctx->buffer_len;
        size_t take = len < need ? len : need;
        memcpy(ctx->buffer + ctx->buffer_len, data, take);
        ctx->buffer_len += take;
        i += take;
        if (ctx->buffer_len == 64) {
            process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }

    while (len - i >= 64) {
        process_block(ctx, data + i);
        i += 64;
    }

    if (i < len) {
        memcpy(ctx->buffer, data + i, len - i);
        ctx->buffer_len = len - i;
    }
}

// Shared by sha256_final/sha224_final: pad, process the final block(s),
// then copy out `num_words` 32-bit state words. SHA-224 and SHA-256 are
// the identical algorithm up to this point -- they differ only in the
// initial hash value (sha224_init vs sha256_init) and in how much of
// the final state they expose (7 words/28 bytes vs 8/32).
static void finalize(SHA256_CTX *ctx, uint8_t *digest, int num_words) {
    uint64_t bit_len = ctx->bit_len; // total length of the ORIGINAL message
    size_t n = ctx->buffer_len;

    // Padding: a single 1 bit (0x80 byte, since input is byte-aligned),
    // then zeros, then the original bit length as a 64-bit big-endian
    // integer, all landing on a 512-bit block boundary.
    ctx->buffer[n++] = 0x80;

    if (n > 56) {
        while (n < 64) ctx->buffer[n++] = 0;
        process_block(ctx, ctx->buffer);
        n = 0;
    }
    while (n < 56) ctx->buffer[n++] = 0;

    for (int i = 0; i < 8; i++) ctx->buffer[56 + i] = (uint8_t)(bit_len >> (56 - 8 * i));
    process_block(ctx, ctx->buffer);

    for (int i = 0; i < num_words; i++) {
        digest[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

void sha256_final(SHA256_CTX *ctx, uint8_t digest[32]) {
    finalize(ctx, digest, 8);
}

void sha256_hash(const uint8_t *data, size_t len, uint8_t digest[32]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

void sha256_to_hex(const uint8_t digest[32], char hex[65]) {
    static const char *lut = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex[i * 2] = lut[digest[i] >> 4];
        hex[i * 2 + 1] = lut[digest[i] & 0x0f];
    }
    hex[64] = '\0';
}

// SHA-224's own initial hash value (FIPS 180-4 §5.3.2) -- different from
// SHA-256's H0, otherwise every other constant (K[]) and the whole
// compression function are shared.
static const uint32_t H0_224[8] = {
    0xc1059ed8, 0x367cd507, 0x3070dd17, 0xf70e5939, 0xffc00b31, 0x68581511, 0x64f98fa7, 0xbefa4fa4,
};

void sha224_init(SHA256_CTX *ctx) {
    memcpy(ctx->state, H0_224, sizeof(H0_224));
    ctx->bit_len = 0;
    ctx->buffer_len = 0;
}

void sha224_final(SHA256_CTX *ctx, uint8_t digest[28]) {
    finalize(ctx, digest, 7); // one word (32 bits) short of SHA-256's output
}

void sha224_hash(const uint8_t *data, size_t len, uint8_t digest[28]) {
    SHA256_CTX ctx;
    sha224_init(&ctx);
    sha256_update(&ctx, data, len); // update is identical to SHA-256's
    sha224_final(&ctx, digest);
}

void sha224_to_hex(const uint8_t digest[28], char hex[57]) {
    static const char *lut = "0123456789abcdef";
    for (int i = 0; i < 28; i++) {
        hex[i * 2] = lut[digest[i] >> 4];
        hex[i * 2 + 1] = lut[digest[i] & 0x0f];
    }
    hex[56] = '\0';
}
