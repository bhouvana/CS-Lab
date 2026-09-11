// fileno()/strsignal() are POSIX (POSIX.1-2008), not ISO C; -std=c11
// alone hides them on glibc -- same issue as compilers/bytecode-vm's
// test file. Must be defined before the first system header.
#define _POSIX_C_SOURCE 200809L
#include "shell.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int shell_tokenize(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;

    for (;;) {
        while (*p != '\0' && isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;

        char *start;
        if (*p == '"' || *p == '\'') {
            char quote = *p;
            p++;
            start = p;
            while (*p != '\0' && *p != quote) p++;
            if (*p == '\0') return -1; // never closed
            *p = '\0';
            p++;
        } else {
            start = p;
            while (*p != '\0' && !isspace((unsigned char)*p)) p++;
            if (*p != '\0') {
                *p = '\0';
                p++;
            }
        }

        if (argc < max_args - 1) argv[argc++] = start; // else: silently dropped
    }

    argv[argc] = NULL;
    return argc;
}

static int builtin_cd(int argc, char **argv) {
    const char *dir = (argc >= 2) ? argv[1] : getenv("HOME");
    if (dir == NULL) dir = "/";
    if (chdir(dir) != 0) {
        fprintf(stderr, "shell: cd: %s: %s\n", dir, strerror(errno));
        return 1;
    }
    return 0;
}

static int builtin_pwd(void) {
    char buf[SHELL_MAX_LINE];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        fprintf(stderr, "shell: pwd: %s\n", strerror(errno));
        return 1;
    }
    printf("%s\n", buf);
    return 0;
}

int shell_exec(int argc, char **argv) {
    if (argc == 0) return 0;

    if (strcmp(argv[0], "exit") == 0) {
        int code = (argc >= 2) ? atoi(argv[1]) : 0;
        exit(code); // does not return
    }
    if (strcmp(argv[0], "cd") == 0) return builtin_cd(argc, argv);
    if (strcmp(argv[0], "pwd") == 0) return builtin_pwd();

    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "shell: fork: %s\n", strerror(errno));
        return 1;
    }
    if (pid == 0) {
        // The shell process ignores SIGINT (see shell_loop) so Ctrl-C at
        // the prompt never kills it; a foreground child must not inherit
        // that -- reset to the default disposition so Ctrl-C still kills
        // (or a signal test still terminates) the child normally.
        signal(SIGINT, SIG_DFL);
        execvp(argv[0], argv);
        // execvp only returns on failure.
        int err = errno;
        fprintf(stderr, "shell: %s: %s\n", argv[0], strerror(err));
        _exit(err == ENOENT ? 127 : 126);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "shell: waitpid: %s\n", strerror(errno));
        return 1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        fprintf(stderr, "shell: %s: terminated by signal %d (%s)\n", argv[0], sig, strsignal(sig));
        return 128 + sig; // real shells' convention for a signal-killed child
    }
    return 1;
}

int shell_loop(FILE *in, FILE *out, int interactive) {
    // The shell survives Ctrl-C at its own prompt; only a running
    // foreground child (which resets this to SIG_DFL after fork, above)
    // actually dies from it.
    signal(SIGINT, SIG_IGN);

    char line[SHELL_MAX_LINE];
    char *argv[SHELL_MAX_ARGS];
    int last_status = 0;

    for (;;) {
        if (interactive) {
            fprintf(out, "$ ");
            fflush(out);
        }
        if (fgets(line, sizeof(line), in) == NULL) {
            if (interactive) fprintf(out, "\n");
            break; // EOF: exit with the last command's status, like a real shell does non-interactively
        }
        line[strcspn(line, "\n")] = '\0';

        int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
        if (argc < 0) {
            fprintf(stderr, "shell: unterminated quote\n");
            last_status = 2;
            continue;
        }
        if (argc == 0) continue; // blank line

        last_status = shell_exec(argc, argv);
    }

    return last_status;
}
