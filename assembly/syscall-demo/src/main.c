// Calls the raw syscall wrappers from src/syscalls.S directly -- this
// program links libc (for the demo's own bookkeeping), but the actual
// write/exit calls never touch libc's write()/exit(), only the
// hand-written syscall trampolines.
#include <stdio.h>

extern long my_write(int fd, const void* buf, unsigned long count);
extern void my_exit(int status) __attribute__((noreturn));

int main(void) {
    const char msg[] = "printed via my_write(), a hand-written syscall wrapper\n";
    long n = my_write(1, msg, sizeof(msg) - 1);
    printf("my_write returned %ld (bytes written)\n", n);

    long bad = my_write(-1, msg, sizeof(msg) - 1);
    printf("my_write(-1, ...) returned %ld (raw kernel -EBADF, not libc's -1+errno)\n", bad);

    printf("about to call my_exit(7) -- process should exit with status 7\n");
    // my_exit() is a raw `syscall`, not libc's exit(): it never flushes
    // stdio buffers. Without this fflush, every printf() above would
    // silently vanish -- confirmed the hard way with strace while
    // building this lab (see README).
    fflush(stdout);
    my_exit(7);
    printf("unreachable\n"); // must never print
    return 1;
}
