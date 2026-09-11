// Experiment: what does a real CALL/RET plus the register-passing
// convention actually cost, compared to the compiler inlining the
// exact same operation away entirely?
#include <stdio.h>
#include <time.h>

extern long add2(long a, long b);

// volatile prevents the compiler from const-folding or hoisting this
// loop away -- we want it to actually execute N additions.
static long add_inline(long a, long b) {
    return a + b;
}

int main(void) {
    const long n = 200 * 1000 * 1000;
    volatile long sink = 0;

    clock_t t0 = clock();
    for (long i = 0; i < n; i++) sink += add2(i, 1); // real CALL/RET each iteration
    double asm_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

    t0 = clock();
    for (long i = 0; i < n; i++) sink += add_inline(i, 1); // -O2 should inline this away
    double inline_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

    printf("Benchmark: CALL/RET overhead vs. an inlinable equivalent (%ld iterations)\n\n", n);
    printf("via add2 (real CALL/RET):     %8.2f ms  (%.2f ns/call)\n", asm_ms, 1e6 * asm_ms / (double)n);
    printf("via add_inline (compiler-inlined): %8.2f ms  (%.2f ns/call)\n", inline_ms,
           1e6 * inline_ms / (double)n);
    printf("sink (ignore): %ld\n", sink);
    return 0;
}
