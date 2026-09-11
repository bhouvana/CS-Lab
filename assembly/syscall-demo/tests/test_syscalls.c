// Plain assert-based tests, no framework.
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern long my_write(int fd, const void* buf, unsigned long count);
extern void my_exit(int status) __attribute__((noreturn));

static void test_write_to_real_file_normal_case(void) {
    const char* path = "tests/tmp_syscall_test.txt";
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);

    const char msg[] = "hello from a raw syscall";
    long n = my_write(fd, msg, sizeof(msg) - 1);
    assert(n == (long)(sizeof(msg) - 1));
    close(fd);

    // Read it back with plain libc I/O to confirm the bytes really
    // landed on disk, not just that my_write() claimed success.
    FILE* f = fopen(path, "r");
    assert(f != NULL);
    char buf[64] = {0};
    size_t read_n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    remove(path);

    assert(read_n == sizeof(msg) - 1);
    assert(memcmp(buf, msg, sizeof(msg) - 1) == 0);
    printf("ok: my_write() writes real bytes to a real file descriptor\n");
}

static void test_write_zero_bytes_edge_case(void) {
    long n = my_write(1, "", 0);
    assert(n == 0);
    printf("ok: my_write(fd, ptr, 0) writes zero bytes and returns 0\n");
}

static void test_write_invalid_fd_returns_raw_negative_errno_invalid_case(void) {
    long result = my_write(-1, "x", 1);
    // Raw kernel ABI: a failed syscall returns -errno directly in RAX,
    // not libc's "-1, with the real code stashed in the errno global."
    assert(result < 0);
    assert(-result == EBADF);
    printf("ok: my_write(-1, ...) returns -EBADF (%ld) directly, the raw kernel convention\n", result);
}

static void test_write_closed_fd_invalid_case(void) {
    int fd = open("tests/tmp_syscall_test2.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    assert(fd >= 0);
    close(fd);
    remove("tests/tmp_syscall_test2.txt");

    long result = my_write(fd, "x", 1); // fd is now stale
    assert(result < 0);
    assert(-result == EBADF);
    printf("ok: writing to a closed fd returns -EBADF\n");
}

static void test_exit_status_via_fork_normal_case(void) {
    // my_exit() terminates the calling process immediately, so this
    // has to happen in a child -- fork(), call my_exit() there, and
    // check the real exit status the kernel recorded for it.
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        my_exit(42);
        _exit(1); // unreachable: only runs if my_exit somehow returned
    }
    int status = 0;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 42);
    printf("ok: my_exit(42) in a child process really exits with status 42\n");
}

int main(void) {
    test_write_to_real_file_normal_case();
    test_write_zero_bytes_edge_case();
    test_write_invalid_fd_returns_raw_negative_errno_invalid_case();
    test_write_closed_fd_invalid_case();
    test_exit_status_via_fork_normal_case();
    printf("all tests passed\n");
    return 0;
}
