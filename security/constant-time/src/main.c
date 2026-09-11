// Correctness demo (the timing EXPERIMENT is `make benchmark` /
// src/bench.c -- this just shows both functions agree on the answer).
#include "compare.h"

#include <stdio.h>
#include <string.h>

static void demo(const char* label, const char* a, const char* b) {
    size_t len = strlen(a);
    int r1 = insecure_compare((const unsigned char*)a, (const unsigned char*)b, len);
    int r2 = constant_time_compare((const unsigned char*)a, (const unsigned char*)b, len);
    printf("%-20s insecure=%d  constant_time=%d\n", label, r1, r2);
}

int main(void) {
    demo("equal", "correct-password", "correct-password");
    demo("differ at byte 0", "correct-password", "Xorrect-password");
    demo("differ at last byte", "correct-password", "correct-passworX");
    return 0;
}
