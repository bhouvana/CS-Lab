// Experiment: how many trivial commands per second can this shell's
// read-tokenize-fork-exec-wait loop actually push through? Every command
// this shell runs (that isn't cd/exit/pwd) costs one fork() + one
// execvp() + one waitpid() -- this measures that full round trip, not
// just a single syscall (see assembly/syscall-demo for that angle).
//
// Drives the real ./shell binary over a pipe rather than calling
// shell_loop() in-process, so the measurement includes exactly what a
// real invocation costs, fork() included.
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

// Runs `n` "true" commands through a fresh ./shell process, piping them
// all in at once and reading nothing back (true never prints), then
// timing until the shell itself exits. Returns elapsed milliseconds.
static double time_n_commands(int n) {
    int in_pipe[2]; // parent write end -> shell's stdin
    if (pipe(in_pipe) != 0) {
        perror("pipe");
        exit(1);
    }

    // fork() duplicates this process's still-buffered stdout (the header
    // and earlier rows this program already printf'd) into the child's
    // memory; without flushing first, that inherited buffer gets written
    // out a second time when the child's own stdio is torn down --
    // exactly the bug networking/tcp-chat's bench.c already documents
    // and works around the same way.
    fflush(stdout);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        close(in_pipe[0]);
        close(in_pipe[1]);
        // discard the shell's own stdout (none expected -- non-interactive,
        // and `true` prints nothing -- but don't rely on that)
        if (freopen("/dev/null", "w", stdout) == NULL) _exit(126);
        execl("./shell", "./shell", (char *)NULL);
        _exit(127);
    }
    close(in_pipe[0]);

    double t0 = now_ms();
    for (int i = 0; i < n; i++) {
        if (write(in_pipe[1], "true\n", 5) != 5) {
            perror("write");
            break;
        }
    }
    dprintf(in_pipe[1], "exit 0\n");
    close(in_pipe[1]);

    int status;
    waitpid(pid, &status, 0);
    return now_ms() - t0;
}

int main(void) {
    printf("Benchmark: shell command-dispatch throughput (fork+exec+wait per command)\n\n");
    printf("%-12s%-14s%s\n", "commands", "elapsed(ms)", "commands/sec");

    int sizes[] = {100, 500, 2000};
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        double ms = time_n_commands(sizes[i]);
        double per_sec = ms > 0.0 ? 1000.0 * (double)sizes[i] / ms : 0.0;
        printf("%-12d%-14.2f%.0f\n", sizes[i], ms, per_sec);
    }
    return 0;
}
