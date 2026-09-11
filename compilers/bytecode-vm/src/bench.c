// Experiments:
//   1. Raw VM throughput on a tight countdown loop, at increasing
//      iteration counts.
//   2. How much does --trace cost, at equal iteration count?
#include "vm.h"

#include <stdlib.h>
#include <time.h>

// counter = n; while (counter != 0) { counter -= 1; } print(counter); halt
static Program build_countdown_program(int64_t n) {
    Instruction *code = malloc(12 * sizeof(Instruction));
    code[0] = (Instruction){OP_PUSH, n};
    code[1] = (Instruction){OP_STORE, 0};
    code[2] = (Instruction){OP_LOAD, 0};          // loop_start
    code[3] = (Instruction){OP_JUMP_IF_FALSE, 9}; // -> end
    code[4] = (Instruction){OP_LOAD, 0};
    code[5] = (Instruction){OP_PUSH, 1};
    code[6] = (Instruction){OP_SUB, 0};
    code[7] = (Instruction){OP_STORE, 0};
    code[8] = (Instruction){OP_JUMP, 2}; // -> loop_start
    code[9] = (Instruction){OP_PUSH, 0}; // end
    code[10] = (Instruction){OP_PRINT, 0};
    code[11] = (Instruction){OP_HALT, 0};
    Program p = {code, 12};
    return p;
}

// clock()'s resolution (notoriously ~15ms on Windows) is too coarse to
// time a single fast run directly, so this repeats the run until at
// least MIN_MS have elapsed in total, then reports the per-run average
// — a real measurement, not a fabricated one, just an averaged one.
// `out` is always a scratch file, never real stdout, so PRINT/trace
// output never pollutes the benchmark table.
#define MIN_MS 50.0

static double run_timed(int64_t n, int trace, FILE *out, int *reps_out) {
    int reps = 0;
    clock_t t0 = clock();
    double elapsed_ms;
    do {
        Program p = build_countdown_program(n);
        VM vm;
        vm_init(&vm, &p);
        vm_run(&vm, trace, out);
        free(p.code);
        reps++;
        elapsed_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
    } while (elapsed_ms < MIN_MS && reps < 10000);
    if (reps_out) *reps_out = reps;
    return elapsed_ms / reps;
}

int main(void) {
    // Not tmpfile(): on Windows it tries to create the file at the
    // drive root and fails without admin rights.
    const char *scratch_path = "vm_bench_scratch.tmp";
    FILE *scratch = fopen(scratch_path, "w");

    printf("Benchmark: raw VM throughput (countdown loop, no trace)\n");
    printf("(each row averaged over enough repeats to exceed %.0fms total)\n\n", MIN_MS);
    printf("%-14s%-8s%-12s%-16s\n", "iterations", "reps", "ms/run", "iterations/sec");
    int64_t sizes[] = {100000, 1000000, 5000000};
    double baseline_ms = 0.0;
    int baseline_reps = 0;
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        int64_t n = sizes[i];
        int reps;
        double ms = run_timed(n, 0, scratch, &reps);
        if (i == 0) {
            baseline_ms = ms;
            baseline_reps = reps;
        }
        printf("%-14lld%-8d%-12.4f%-16.0f\n", (long long)n, reps, ms, (double)n / (ms / 1000.0));
    }

    printf("\nBenchmark: --trace overhead at %lld iterations\n", (long long)sizes[0]);
    printf("(output redirected to a file, not the terminal)\n\n");
    int trace_reps;
    double trace_ms = run_timed(sizes[0], 1, scratch, &trace_reps);
    printf("no trace: %8.4f ms/run (%d reps)\n", baseline_ms, baseline_reps);
    printf("trace:    %8.4f ms/run (%d reps)  -> %.1fx slower\n", trace_ms, trace_reps, trace_ms / baseline_ms);

    fclose(scratch);
    remove(scratch_path);
    return 0;
}
