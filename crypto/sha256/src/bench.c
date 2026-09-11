// Experiment: hashing throughput as input size grows.
#include "sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// clock()'s resolution is too coarse to time a single hash of a small
// input directly, so small inputs are hashed repeatedly until at least
// MIN_MS have elapsed, and the per-hash average is reported.
#define MIN_MS 50.0

static double time_hash_ms(const uint8_t *data, size_t n, int *reps_out) {
    int reps = 0;
    clock_t t0 = clock();
    double elapsed_ms;
    uint8_t digest[32];
    do {
        sha256_hash(data, n, digest);
        reps++;
        elapsed_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
    } while (elapsed_ms < MIN_MS && reps < 100000);
    *reps_out = reps;
    return elapsed_ms / reps;
}

int main(void) {
    printf("Benchmark: SHA-256 throughput by input size\n");
    printf("(each row averaged over enough repeats to exceed %.0fms total)\n\n", MIN_MS);
    printf("%-10s%-8s%-12s%-10s\n", "input", "reps", "ms/hash", "MB/s");

    size_t sizes[] = {1024, 1024 * 1024, 16 * 1024 * 1024};
    const char *labels[] = {"1 KB", "1 MB", "16 MB"};

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        size_t n = sizes[i];
        uint8_t *data = malloc(n);
        for (size_t j = 0; j < n; j++) data[j] = (uint8_t)(j * 2654435761u); // arbitrary reproducible fill

        int reps;
        double ms = time_hash_ms(data, n, &reps);
        double mb_per_s = ms > 0 ? (double)n / 1e6 / (ms / 1000.0) : 0;
        printf("%-10s%-8d%-12.4f%-10.1f\n", labels[i], reps, ms, mb_per_s);
        free(data);
    }
    return 0;
}
