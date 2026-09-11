// Experiment: how does collection time scale with heap size and with
// the fraction of the heap that's live vs. garbage?
#include "gc.h"

#include <stdio.h>
#include <time.h>

static void build_heap(GC* gc, int n, int live_count) {
    Object* prev_live = NULL;
    for (int i = 0; i < live_count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "live%d", i);
        Object* o = gc_new_object(gc, name);
        if (prev_live) gc_add_reference(prev_live, o);
        else gc_add_root(gc, o);
        prev_live = o;
    }
    for (int i = 0; i < n - live_count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "garbage%d", i);
        gc_new_object(gc, name); // unrooted, unreferenced: pure garbage
    }
}

// clock()'s resolution is too coarse to time a single fast collection
// directly, so this rebuilds and re-collects the same heap repeatedly
// until at least MIN_MS have elapsed, then reports the per-run average.
#define MIN_MS 20.0

static double time_collect_ms(int n, int live_count, int* reps_out) {
    int reps = 0;
    clock_t t0 = clock();
    double elapsed_ms;
    do {
        GC gc;
        gc_init(&gc);
        build_heap(&gc, n, live_count);
        gc_collect(&gc);
        gc_destroy(&gc);
        reps++;
        elapsed_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
    } while (elapsed_ms < MIN_MS && reps < 10000);
    *reps_out = reps;
    return elapsed_ms / reps;
}

int main(void) {
    printf("Benchmark: collection time vs. heap size and live/garbage ratio\n");
    printf("(the live set is one long reference chain -- see README on why\n");
    printf("mark() is iterative, not recursive, specifically because of this)\n\n");
    printf("%-12s%-10s%-8s%-10s\n", "objects", "% live", "reps", "ms/run");
    int sizes[] = {1000, 10000, 100000};
    int live_pct[] = {10, 50, 90};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        for (size_t j = 0; j < sizeof(live_pct) / sizeof(live_pct[0]); j++) {
            int n = sizes[i];
            int live_count = n * live_pct[j] / 100;
            int reps;
            double ms = time_collect_ms(n, live_count, &reps);
            printf("%-12d%-10d%-8d%-10.4f\n", n, live_pct[j], reps, ms);
        }
    }
    return 0;
}
