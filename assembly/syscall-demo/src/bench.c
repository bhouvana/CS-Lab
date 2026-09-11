// Experiment: how much does crossing into the kernel actually cost,
// compared to a plain userspace function call?
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

extern long my_getpid(void);
extern long my_write(int fd, const void* buf, unsigned long count);

static volatile long sink = 0;
static long noop_call(long x) {
    return x + 1; // no syscall, just a plain call+add -- the userspace baseline
}

int main(void) {
    const char* iteration_text = getenv("SYSCALL_BENCH_ITERATIONS");
    const long n = iteration_text == NULL ? 1000 * 1000 : atol(iteration_text);
    if (n <= 0) {
        fprintf(stderr, "SYSCALL_BENCH_ITERATIONS must be positive\n");
        return 1;
    }
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull < 0) {
        perror("open /dev/null");
        return 1;
    }
    char byte = 'x';
    int real_file = open("syscall_bench.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (real_file < 0) {
        perror("open syscall_bench.tmp");
        close(devnull);
        return 1;
    }

    clock_t t0 = clock();
    for (long i = 0; i < n; i++) sink = my_getpid();
    double getpid_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

    t0 = clock();
    for (long i = 0; i < n; i++) sink = my_write(devnull, &byte, 1); // real syscall each time
    double syscall_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

    t0 = clock();
    for (long i = 0; i < n; i++) sink = my_write(real_file, &byte, 1);
    double real_file_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

    t0 = clock();
    for (long i = 0; i < n; i++) sink = noop_call(i); // pure userspace, no kernel crossing
    double call_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

    close(devnull);
    close(real_file);
    unlink("syscall_bench.tmp");

    printf("Benchmark: syscall cost by kernel workload (%ld iterations)\n\n", n);
    printf("my_getpid (no arguments):  %8.2f ms  (%.1f ns/call)\n", getpid_ms, 1e6 * getpid_ms / (double)n);
    printf("my_write(devnull, ...):  %8.2f ms  (%.1f ns/call)\n", syscall_ms, 1e6 * syscall_ms / (double)n);
    printf("my_write(real file, ...): %8.2f ms  (%.1f ns/call)\n", real_file_ms, 1e6 * real_file_ms / (double)n);
    printf("noop_call (no syscall):  %8.2f ms  (%.1f ns/call)\n", call_ms, 1e6 * call_ms / (double)n);
    printf("getpid/write(devnull) ratio: %.2fx\n", getpid_ms / syscall_ms);
    printf("write(real file)/write(devnull) ratio: %.2fx\n", real_file_ms / syscall_ms);
    printf("write(devnull)/noop ratio: %.0fx\n", syscall_ms / call_ms);
    printf("sink (ignore): %ld\n", sink);
    return 0;
}
