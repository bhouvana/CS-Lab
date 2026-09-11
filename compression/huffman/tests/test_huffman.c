// Plain assert-based tests, no framework.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "huffman.h"

static void roundtrip(const uint8_t *data, size_t len, const char *label) {
    uint8_t *compressed;
    size_t compressed_len;
    assert(huffman_compress_buffer(data, len, &compressed, &compressed_len) == 0);

    uint8_t *decompressed;
    size_t decompressed_len;
    assert(huffman_decompress_buffer(compressed, compressed_len, &decompressed, &decompressed_len) == 0);

    assert(decompressed_len == len);
    assert(len == 0 || memcmp(data, decompressed, len) == 0);

    printf("ok: %-20s %lu -> %lu bytes\n", label, (unsigned long)len, (unsigned long)compressed_len);
    free(compressed);
    free(decompressed);
}

static void test_normal_english_text(void) {
    const char *text = "the quick brown fox jumps over the lazy dog the fox runs";
    roundtrip((const uint8_t *)text, strlen(text), "normal (english)");
}

static void test_edge_empty_input(void) {
    roundtrip((const uint8_t *)"", 0, "edge (empty)");
}

static void test_edge_single_symbol_repeated(void) {
    uint8_t data[500];
    memset(data, 'A', sizeof(data));
    roundtrip(data, sizeof(data), "edge (1 symbol)");
}

static void test_edge_two_symbols(void) {
    const char *text = "ABABABABABABABAB";
    roundtrip((const uint8_t *)text, strlen(text), "edge (2 symbols)");
}

static void test_invalid_corrupt_magic(void) {
    uint8_t bad[268] = {0};
    memcpy(bad, "XXXX", 4);
    uint8_t *out;
    size_t out_len;
    assert(huffman_decompress_buffer(bad, sizeof(bad), &out, &out_len) == -1);
    printf("ok: invalid (bad magic) rejected\n");
}

static void test_invalid_truncated_header(void) {
    uint8_t bad[10] = {0};
    uint8_t *out;
    size_t out_len;
    assert(huffman_decompress_buffer(bad, sizeof(bad), &out, &out_len) == -1);
    printf("ok: invalid (truncated header) rejected\n");
}

static void test_regression_all_256_byte_values(void) {
    uint8_t data[256];
    for (int i = 0; i < 256; i++) data[i] = (uint8_t)i;
    roundtrip(data, sizeof(data), "regression (all bytes)");
}

int main(void) {
    test_normal_english_text();
    test_edge_empty_input();
    test_edge_single_symbol_repeated();
    test_edge_two_symbols();
    test_invalid_corrupt_magic();
    test_invalid_truncated_header();
    test_regression_all_256_byte_values();
    printf("all tests passed\n");
    return 0;
}
