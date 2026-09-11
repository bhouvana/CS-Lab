// Two layers: direct unit tests against shell_tokenize() (linked
// straight into this binary, no process needed), and integration tests
// that fork()+exec() the real ./shell binary and drive it over a pipe --
// same pattern as networking/tcp-chat's test_chat.c.
#define _POSIX_C_SOURCE 200809L
#include "shell.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// usleep() was dropped from POSIX.1-2008 (nanosleep() replaces it); a
// tiny wrapper keeps the two call sites below readable.
static void sleep_ms(long ms) {
    struct timespec ts = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

// ---- shell_tokenize() unit tests -------------------------------------

static void test_tokenize_normal_case(void) {
    char line[] = "ls -la";
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
    assert(argc == 2);
    assert(strcmp(argv[0], "ls") == 0);
    assert(strcmp(argv[1], "-la") == 0);
    assert(argv[2] == NULL);
    printf("ok: tokenize splits on whitespace\n");
}

static void test_tokenize_extra_whitespace_normal_case(void) {
    char line[] = "  ls    -la  ";
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
    assert(argc == 2);
    assert(strcmp(argv[0], "ls") == 0);
    assert(strcmp(argv[1], "-la") == 0);
    printf("ok: tokenize collapses repeated/leading/trailing whitespace\n");
}

static void test_tokenize_double_quoted_spaces_normal_case(void) {
    char line[] = "echo \"hello world\"";
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
    assert(argc == 2);
    assert(strcmp(argv[0], "echo") == 0);
    assert(strcmp(argv[1], "hello world") == 0); // one token, not two
    printf("ok: a double-quoted argument with a space is one token\n");
}

static void test_tokenize_single_quoted_spaces_normal_case(void) {
    char line[] = "echo 'a b c'";
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
    assert(argc == 2);
    assert(strcmp(argv[1], "a b c") == 0);
    printf("ok: single quotes work the same way as double quotes\n");
}

static void test_tokenize_empty_edge_case(void) {
    char line[] = "";
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
    assert(argc == 0);
    assert(argv[0] == NULL);
    printf("ok: an empty line tokenizes to 0 arguments\n");
}

static void test_tokenize_whitespace_only_edge_case(void) {
    char line[] = "    \t  ";
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
    assert(argc == 0);
    printf("ok: a whitespace-only line tokenizes to 0 arguments\n");
}

static void test_tokenize_unterminated_double_quote_invalid_case(void) {
    char line[] = "echo \"never closed";
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
    assert(argc == -1);
    printf("ok: an unterminated double quote is rejected\n");
}

static void test_tokenize_unterminated_single_quote_invalid_case(void) {
    char line[] = "echo 'never closed";
    char *argv[SHELL_MAX_ARGS];
    int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
    assert(argc == -1);
    printf("ok: an unterminated single quote is rejected\n");
}

static void test_tokenize_over_max_args_edge_case(void) {
    char line[] = "a b c d e";
    char *argv[3]; // room for only 2 tokens + the NULL terminator
    int argc = shell_tokenize(line, argv, 3);
    assert(argc == 2); // "c d e" silently dropped, not overflowed into argv
    assert(strcmp(argv[0], "a") == 0);
    assert(strcmp(argv[1], "b") == 0);
    assert(argv[2] == NULL);
    printf("ok: tokens beyond max_args are dropped, not overflowed\n");
}

// ---- ./shell integration tests ---------------------------------------

// Forks the real ./shell binary, feeds it `script` over a pipe, and
// returns its exit status. `*out_buf` (size `out_size`) collects
// everything the shell wrote to stdout.
static int run_shell(const char *script, char *out_buf, size_t out_size, pid_t *pid_out) {
    int in_pipe[2], out_pipe[2];
    assert(pipe(in_pipe) == 0);
    assert(pipe(out_pipe) == 0);

    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        execl("./shell", "./shell", (char *)NULL);
        _exit(127);
    }
    close(in_pipe[0]);
    close(out_pipe[1]);

    if (pid_out != NULL) *pid_out = pid;

    size_t script_len = strlen(script);
    assert(write(in_pipe[1], script, script_len) == (ssize_t)script_len);
    close(in_pipe[1]);

    size_t total = 0;
    ssize_t n;
    while (total + 1 < out_size && (n = read(out_pipe[0], out_buf + total, out_size - 1 - total)) > 0) {
        total += (size_t)n;
    }
    out_buf[total] = '\0';
    close(out_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void test_echo_and_explicit_exit_code_normal_case(void) {
    char out[256];
    int status = run_shell("echo hello\nexit 42\n", out, sizeof(out), NULL);
    assert(strstr(out, "hello") != NULL);
    assert(status == 42);
    printf("ok: echo runs, exit <code> sets the process exit status\n");
}

static void test_command_not_found_invalid_case(void) {
    char out[256];
    int status = run_shell("this_command_does_not_exist_xyz\nexit 7\n", out, sizeof(out), NULL);
    assert(status == 7); // the shell kept going after the failed command
    printf("ok: an unknown command is reported, not a crash, and the shell keeps running\n");
}

static void test_cd_invalid_directory_invalid_case(void) {
    char out[256];
    int status = run_shell("cd /no/such/directory/at/all\necho still-alive\nexit 0\n", out, sizeof(out), NULL);
    assert(strstr(out, "still-alive") != NULL); // cd's failure didn't kill the shell
    assert(status == 0);
    printf("ok: cd into a nonexistent directory fails cleanly, shell continues\n");
}

static void test_eof_without_exit_edge_case(void) {
    char out[256];
    int status = run_shell("true\n", out, sizeof(out), NULL); // no "exit" -- input just ends
    assert(status == 0); // exits with the last command's status on EOF, like a real shell
    printf("ok: EOF with no explicit exit ends the shell using the last command's status\n");
}

static void test_last_exit_overrides_earlier_failure_normal_case(void) {
    char out[256];
    int status = run_shell("false\necho done\nexit 5\n", out, sizeof(out), NULL);
    assert(strstr(out, "done") != NULL);
    assert(status == 5); // the explicit exit code, not false's leftover status
    printf("ok: exit <code> overrides whatever the previous command returned\n");
}

static void test_sigint_is_survived_regression(void) {
    // Regression: a shell that doesn't ignore SIGINT dies at its own
    // prompt the moment Ctrl-C is pressed, same failure class as
    // networking/tcp-chat's SIGPIPE bug -- a signal the process didn't
    // ask for terminating it by default. Sends SIGINT directly to the
    // shell process, then proves it's still alive and working.
    int in_pipe[2], out_pipe[2];
    assert(pipe(in_pipe) == 0);
    assert(pipe(out_pipe) == 0);

    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        execl("./shell", "./shell", (char *)NULL);
        _exit(127);
    }
    close(in_pipe[0]);
    close(out_pipe[1]);

    sleep_ms(50); // let the shell reach its blocking fgets() before signaling it
    assert(kill(pid, SIGINT) == 0);
    sleep_ms(50);

    const char *script = "echo alive\nexit 0\n";
    size_t script_len = strlen(script);
    assert(write(in_pipe[1], script, script_len) == (ssize_t)script_len);
    close(in_pipe[1]);

    char out[128];
    size_t total = 0;
    ssize_t n;
    while (total + 1 < sizeof(out) && (n = read(out_pipe[0], out + total, sizeof(out) - 1 - total)) > 0) {
        total += (size_t)n;
    }
    out[total] = '\0';
    close(out_pipe[0]);

    int status;
    waitpid(pid, &status, 0);
    assert(strstr(out, "alive") != NULL);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    printf("ok: SIGINT at the prompt doesn't kill the shell (SIGINT regression)\n");
}

int main(void) {
    test_tokenize_normal_case();
    test_tokenize_extra_whitespace_normal_case();
    test_tokenize_double_quoted_spaces_normal_case();
    test_tokenize_single_quoted_spaces_normal_case();
    test_tokenize_empty_edge_case();
    test_tokenize_whitespace_only_edge_case();
    test_tokenize_unterminated_double_quote_invalid_case();
    test_tokenize_unterminated_single_quote_invalid_case();
    test_tokenize_over_max_args_edge_case();

    test_echo_and_explicit_exit_code_normal_case();
    test_command_not_found_invalid_case();
    test_cd_invalid_directory_invalid_case();
    test_eof_without_exit_edge_case();
    test_last_exit_overrides_earlier_failure_normal_case();
    test_sigint_is_survived_regression();

    printf("all tests passed\n");
    return 0;
}
