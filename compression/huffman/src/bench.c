// Experiment: which kinds of data compress well, and why does random
// data not?
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "huffman.h"

static int read_whole_file(const char *path, uint8_t **buf, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *buf = malloc((size_t)size);
    if (fread(*buf, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        return -1;
    }
    fclose(f);
    *len = (size_t)size;
    return 0;
}

// clock()'s resolution is too coarse (notoriously ~15ms on Windows) to
// time a single compress/decompress of a small file directly -- it was
// observed reading a flat 0.000ms for every dataset here on a native
// Windows/MinGW build. Repeat until at least MIN_MS have elapsed and
// report the per-run average, same fix already applied in the other
// labs' bench.c (sha256, bytecode-vm, garbage-collector).
#define MIN_MS 50.0

static void bench_one(const char *label, const char *path) {
    uint8_t *data;
    size_t len;
    if (read_whole_file(path, &data, &len) != 0) {
        fprintf(stderr, "skip %s: cannot open %s\n", label, path);
        return;
    }

    int reps = 0;
    clock_t t0 = clock();
    double encode_ms;
    uint8_t *compressed = NULL;
    size_t compressed_len = 0;
    do {
        free(compressed);
        huffman_compress_buffer(data, len, &compressed, &compressed_len);
        reps++;
        encode_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
    } while (encode_ms < MIN_MS && reps < 100000);
    encode_ms /= reps;

    reps = 0;
    t0 = clock();
    double decode_ms;
    uint8_t *decompressed = NULL;
    size_t decompressed_len = 0;
    do {
        free(decompressed);
        huffman_decompress_buffer(compressed, compressed_len, &decompressed, &decompressed_len);
        reps++;
        decode_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
    } while (decode_ms < MIN_MS && reps < 100000);
    decode_ms /= reps;

    double ratio = len > 0 ? 100.0 * (double)compressed_len / (double)len : 0.0;
    printf("%-12s %10lu %14lu %9.2f%% %10.3f %10.3f\n", label, (unsigned long)len, (unsigned long)compressed_len, ratio,
           encode_ms, decode_ms);

    free(data);
    free(compressed);
    free(decompressed);
}

// Compresses `data` directly (no file I/O) and reports the ratio --
// used by the two experiments below, which build their inputs in
// memory rather than reading example files.
static void bench_ratio_only(const char *label, const uint8_t *data, size_t len) {
    uint8_t *compressed = NULL;
    size_t compressed_len = 0;
    huffman_compress_buffer(data, len, &compressed, &compressed_len);
    double ratio = len > 0 ? 100.0 * (double)compressed_len / (double)len : 0.0;
    printf("%-16s %12lu %14lu %9.2f%%\n", label, (unsigned long)len, (unsigned long)compressed_len, ratio);
    free(compressed);
}

int main(void) {
    printf("Benchmark: compression ratio by data type\n\n");
    printf("%-12s %10s %14s %10s %10s %10s\n", "dataset", "original", "compressed", "ratio", "encode(ms)", "decode(ms)");
    bench_one("english", "examples/english.txt");
    bench_one("repetitive", "examples/repetitive.txt");
    bench_one("source_code", "examples/source.c");
    bench_one("random", "examples/random.bin");

    // --- Does the 268-byte header disappear into the noise on a much
    // larger English corpus? Built by repeating examples/english.txt in
    // memory (no new example file to keep in the repo) -- the frequency
    // distribution stays representative of real English either way,
    // since it's the same underlying text, just repeated.
    printf("\nBenchmark: does header overhead disappear on a larger English corpus?\n\n");
    printf("%-16s %12s %14s %10s\n", "size", "original", "compressed", "ratio");
    {
        uint8_t *english;
        size_t english_len;
        if (read_whole_file("examples/english.txt", &english, &english_len) == 0) {
            bench_ratio_only("1x (baseline)", english, english_len);
            int multipliers[] = {10, 100, 1000};
            for (size_t m = 0; m < sizeof(multipliers) / sizeof(multipliers[0]); m++) {
                size_t big_len = english_len * (size_t)multipliers[m];
                uint8_t *big = malloc(big_len);
                for (int r = 0; r < multipliers[m]; r++) memcpy(big + (size_t)r * english_len, english, english_len);
                char label[32];
                snprintf(label, sizeof(label), "%dx", multipliers[m]);
                bench_ratio_only(label, big, big_len);
                free(big);
            }
            free(english);
        }
    }

    // --- Ratio vs. file size for the same repetitive pattern -- where
    // does the fixed header stop mattering?
    printf("\nBenchmark: ratio vs. file size, same repetitive pattern (\"AAAAAAAAB\" repeated)\n\n");
    printf("%-16s %12s %14s %10s\n", "size", "original", "compressed", "ratio");
    {
        size_t sizes[] = {100, 1000, 10000, 100000, 1000000};
        for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            size_t len = sizes[s];
            uint8_t *data = malloc(len);
            static const char pattern[] = "AAAAAAAAB";
            for (size_t i = 0; i < len; i++) data[i] = (uint8_t)pattern[i % (sizeof(pattern) - 1)];
            char label[32];
            snprintf(label, sizeof(label), "%lu bytes", (unsigned long)len);
            bench_ratio_only(label, data, len);
            free(data);
        }
    }

    return 0;
}
