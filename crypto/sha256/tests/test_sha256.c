// Plain assert-based tests against FIPS 180-4 / well-known test vectors.
#include "sha256.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(const char *label, const uint8_t *data, size_t len, const char *expected_hex) {
    uint8_t digest[32];
    sha256_hash(data, len, digest);
    char hex[65];
    sha256_to_hex(digest, hex);
    if (strcmp(hex, expected_hex) != 0) {
        fprintf(stderr, "FAIL %s:\n  got      %s\n  expected %s\n", label, hex, expected_hex);
        assert(0 && "digest mismatch");
    }
    printf("ok: %-24s %s\n", label, hex);
}

static void test_empty_string_official_vector(void) {
    check("empty string", (const uint8_t *)"", 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

static void test_abc_official_vector(void) {
    check("\"abc\"", (const uint8_t *)"abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

static void test_hello_world(void) {
    check("\"hello world\"", (const uint8_t *)"hello world", 11,
          "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
}

// The classic 56-byte (448-bit) NIST test vector. After the 0x80 padding
// byte this needs 57 bytes before the length field, which no longer
// fits in one 64-byte block (57 > 56) — this is what forces
// sha256_final's two-block padding path, so it's the regression test
// that would catch a padding-boundary bug.
static void test_448_bit_nist_vector_regression(void) {
    const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    check("448-bit NIST vector", (const uint8_t *)msg, strlen(msg),
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

static void test_incremental_update_matches_one_shot(void) {
    const char *text = "the quick brown fox jumps over the lazy dog";
    uint8_t one_shot[32];
    sha256_hash((const uint8_t *)text, strlen(text), one_shot);

    SHA256_CTX ctx;
    sha256_init(&ctx);
    // Feed it in small, uneven chunks to exercise the buffering logic.
    sha256_update(&ctx, (const uint8_t *)text, 5);
    sha256_update(&ctx, (const uint8_t *)text + 5, 20);
    sha256_update(&ctx, (const uint8_t *)text + 25, strlen(text) - 25);
    uint8_t incremental[32];
    sha256_final(&ctx, incremental);

    assert(memcmp(one_shot, incremental, 32) == 0);
    printf("ok: incremental update matches one-shot hash\n");
}

static void test_block_boundary_edge_case(void) {
    // Exactly 55 bytes: the 0x80 padding byte lands at index 55, still
    // fits before the length field in a single block (55+1+8=64).
    uint8_t msg[55];
    memset(msg, 'x', sizeof(msg));
    uint8_t digest1[32], digest2[32];
    sha256_hash(msg, sizeof(msg), digest1);
    sha256_hash(msg, sizeof(msg), digest2); // determinism check
    assert(memcmp(digest1, digest2, 32) == 0);

    // Exactly 56 bytes: 0x80 makes 57, which no longer fits before the
    // length field (56+1+8=65 > 64), forcing the two-block padding path.
    uint8_t msg56[56];
    memset(msg56, 'x', sizeof(msg56));
    uint8_t digest3[32];
    sha256_hash(msg56, sizeof(msg56), digest3);
    assert(memcmp(digest1, digest3, 32) != 0); // sanity: different inputs, different digests
    printf("ok: block-boundary lengths (55 vs 56 bytes) both produce stable digests\n");
}

int main(void) {
    test_empty_string_official_vector();
    test_abc_official_vector();
    test_hello_world();
    test_448_bit_nist_vector_regression();
    test_incremental_update_matches_one_shot();
    test_block_boundary_edge_case();
    printf("all tests passed\n");
    return 0;
}
