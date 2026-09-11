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

static void bench_one(const char *label, const char *path) {
    uint8_t *data;
    size_t len;
    if (read_whole_file(path, &data, &len) != 0) {
        fprintf(stderr, "skip %s: cannot open %s\n", label, path);
        return;
    }

    clock_t t0 = clock();
    uint8_t *compressed;
    size_t compressed_len;
    huffman_compress_buffer(data, len, &compressed, &compressed_len);
    double encode_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

    t0 = clock();
    uint8_t *decompressed;
    size_t decompressed_len;
    huffman_decompress_buffer(compressed, compressed_len, &decompressed, &decompressed_len);
    double decode_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

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
