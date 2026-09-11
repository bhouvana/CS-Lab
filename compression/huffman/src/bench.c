// Experiment: which kinds of data compress well, and why does random
// data not?
#include <stdio.h>
#include <stdlib.h>
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

int main(void) {
    printf("Benchmark: compression ratio by data type\n\n");
    printf("%-12s %10s %14s %10s %10s %10s\n", "dataset", "original", "compressed", "ratio", "encode(ms)", "decode(ms)");
    bench_one("english", "examples/english.txt");
    bench_one("repetitive", "examples/repetitive.txt");
    bench_one("source_code", "examples/source.c");
    bench_one("random", "examples/random.bin");
    return 0;
}
