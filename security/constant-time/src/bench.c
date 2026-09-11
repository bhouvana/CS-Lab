// Experiment: does insecure_compare()'s running time leak WHERE two
// buffers first differ? Does constant_time_compare()'s not?
#include "compare.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define SECRET_LEN 32
#define ITERATIONS 2000000

static const unsigned char secret[SECRET_LEN] = "the-real-secret-value-shhh!!!!!"; // 32 bytes incl. padding

// Builds a guess that matches `secret` for its first `match_len`
// bytes, then differs (or, if match_len == SECRET_LEN, matches
// entirely).
static void build_guess(unsigned char* guess, int match_len) {
    memcpy(guess, secret, SECRET_LEN);
    if (match_len < SECRET_LEN) guess[match_len] ^= 0xFF; // flip the first mismatching byte
}

static double time_calls(int (*cmp)(const unsigned char*, const unsigned char*, size_t), const unsigned char* guess) {
    clock_t t0 = clock();
    volatile int sink = 0;
    for (int i = 0; i < ITERATIONS; i++) sink = cmp(secret, guess, SECRET_LEN);
    (void)sink;
    return 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
}

int main(void) {
    printf("Benchmark: does comparison time leak the mismatch position?\n");
    printf("(%d calls per row; secret is %d bytes)\n\n", ITERATIONS, SECRET_LEN);
    printf("%-16s%-16s%-16s\n", "match_len", "insecure (ms)", "constant-time (ms)");

    int positions[] = {0, 4, 8, 12, 16, 20, 24, 28, 31, 32}; // 32 = full match, no mismatch at all
    for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); i++) {
        unsigned char guess[SECRET_LEN];
        build_guess(guess, positions[i]);
        double insecure_ms = time_calls(insecure_compare, guess);
        double const_ms = time_calls(constant_time_compare, guess);
        printf("%-16d%-16.2f%-16.2f\n", positions[i], insecure_ms, const_ms);
    }
    return 0;
}
