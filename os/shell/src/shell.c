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

static int run_external_child(char **argv) {
    signal(SIGINT, SIG_DFL);
    execvp(argv[0], argv);
    int err = errno;
    fprintf(stderr, "shell: %s: %s\n", argv[0], strerror(err));
    return err == ENOENT ? 127 : 126;
}

static char *trim_spaces(char *text) {
    while (*text != '\0' && isspace((unsigned char)*text)) text++;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
    return text;
}

static int shell_exec_pipeline(char *line) {
    char *commands[SHELL_MAX_ARGS];
    int command_count = 0;
    int quote = 0;
    char *start = line;

    for (char *p = line;; p++) {
        if (*p == '\'' || *p == '"') {
            if (quote == 0) quote = *p;
            else if (quote == *p) quote = 0;
        } else if ((*p == '|' || *p == '\0') && quote == 0) {
            if (command_count >= SHELL_MAX_ARGS) return 2;
            char separator = *p;
            *p = '\0';
            commands[command_count++] = trim_spaces(start);
            if (commands[command_count - 1][0] == '\0') return 2;
            if (separator == '\0') break;
            start = p + 1;
        }
    }
    if (quote != 0) return -1;
    if (command_count == 1) {
        char *argv[SHELL_MAX_ARGS];
        int argc = shell_tokenize(commands[0], argv, SHELL_MAX_ARGS);
        if (argc < 0) return -1;
        return shell_exec(argc, argv);
    }

    int pipes[SHELL_MAX_ARGS - 1][2];
    for (int i = 0; i < command_count - 1; i++) {
        if (pipe(pipes[i]) != 0) {
            fprintf(stderr, "shell: pipe: %s\n", strerror(errno));
            return 1;
        }
    }

    pid_t pids[SHELL_MAX_ARGS];
    for (int i = 0; i < command_count; i++) {
        char *argv[SHELL_MAX_ARGS];
        int argc = shell_tokenize(commands[i], argv, SHELL_MAX_ARGS);
        if (argc <= 0) return argc < 0 ? -1 : 2;
        pids[i] = fork();
        if (pids[i] < 0) return 1;
        if (pids[i] == 0) {
            if (i > 0) dup2(pipes[i - 1][0], STDIN_FILENO);
            if (i < command_count - 1) dup2(pipes[i][1], STDOUT_FILENO);
            for (int j = 0; j < command_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            if (strcmp(argv[0], "cd") == 0 || strcmp(argv[0], "pwd") == 0 || strcmp(argv[0], "exit") == 0) {
                int builtin_status = shell_exec(argc, argv);
                fflush(stdout);
                fflush(stderr);
                _exit(builtin_status);
            }
            _exit(run_external_child(argv));
        }
    }
    for (int i = 0; i < command_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    int last_status = 1;
    for (int i = 0; i < command_count; i++) {
        int status;
        if (waitpid(pids[i], &status, 0) < 0) return 1;
        if (i == command_count - 1) {
            if (WIFEXITED(status)) last_status = WEXITSTATUS(status);
            else if (WIFSIGNALED(status)) last_status = 128 + WTERMSIG(status);
        }
    }
    return last_status;
}

static void expand_status(char *line, int previous_status) {
    char expanded[SHELL_MAX_LINE];
    char status_text[4];
    snprintf(status_text, sizeof(status_text), "%d", previous_status);
    size_t used = 0;
    for (size_t i = 0; line[i] != '\0' && used + 1 < sizeof(expanded); i++) {
        if (line[i] == '$' && line[i + 1] == '?') {
            size_t n = strlen(status_text);
            if (used + n >= sizeof(expanded)) break;
            memcpy(expanded + used, status_text, n);
            used += n;
            i++;
        } else {
            expanded[used++] = line[i];
        }
    }
    expanded[used] = '\0';
    strcpy(line, expanded);
}

int shell_exec_line(char *line, int previous_status) {
    expand_status(line, previous_status);
    int status = previous_status;
    int should_run = 1;
    int quote = 0;
    char *start = line;

    for (char *p = line;; p++) {
        if (*p == '\'' || *p == '"') {
            if (quote == 0) quote = *p;
            else if (quote == *p) quote = 0;
        }
        int operator = 0;
        if (quote == 0 && p[0] != '\0' && p[1] != '\0') {
            if (p[0] == '&' && p[1] == '&') operator = 1;
            if (p[0] == '|' && p[1] == '|') operator = 2;
        }
        if (operator != 0 || *p == '\0') {
            char saved = *p;
            *p = '\0';
            char *command = trim_spaces(start);
            if (should_run) {
                status = shell_exec_pipeline(command);
                if (status < 0) {
                    fprintf(stderr, "shell: unterminated quote\n");
                    status = 2;
                }
            }
            if (saved == '\0') break;
            should_run = (operator == 1) ? (status == 0) : (status != 0);
            p++;
            start = p + 1;
        }
    }
    return status;
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
    if (pid == 0) _exit(run_external_child(argv));

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

        if (line[0] == '\0') continue;
        last_status = shell_exec_line(line, last_status);
    }

    return last_status;
}
