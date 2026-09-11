// Experiment: how much does crossing into the kernel actually cost,
// compared to a plain userspace function call?
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

extern long my_write(int fd, const void* buf, unsigned long count);

static volatile long sink = 0;
static long noop_call(long x) {
    return x + 1; // no syscall, just a plain call+add -- the userspace baseline
}

int main(void) {
    const long n = 1000 * 1000;
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull < 0) {
        perror("open /dev/null");
        return 1;
    }
    char byte = 'x';

    clock_t t0 = clock();
    for (long i = 0; i < n; i++) sink = my_write(devnull, &byte, 1); // real syscall each time
    double syscall_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

    t0 = clock();
    for (long i = 0; i < n; i++) sink = noop_call(i); // pure userspace, no kernel crossing
    double call_ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;

    close(devnull);

    printf("Benchmark: syscall overhead vs. a plain userspace call (%ld iterations)\n\n", n);
    printf("my_write(devnull, ...):  %8.2f ms  (%.1f ns/call)\n", syscall_ms, 1e6 * syscall_ms / (double)n);
    printf("noop_call (no syscall):  %8.2f ms  (%.1f ns/call)\n", call_ms, 1e6 * call_ms / (double)n);
    printf("ratio: syscall is %.0fx the cost of a plain call\n", syscall_ms / call_ms);
    printf("sink (ignore): %ld\n", sink);
    return 0;
}
