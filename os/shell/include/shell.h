#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>

#define SHELL_MAX_ARGS 64
#define SHELL_MAX_LINE 4096

// Splits `line` into whitespace-separated tokens in `argv` (NUL-terminated
// strings, pointing into `line` itself -- `line` must outlive `argv`),
// honoring single- and double-quoted substrings as one token each (so
// `echo "a b"` is 2 tokens, not 3). Mutates `line` in place (quote/space
// delimiters become '\0'). Returns the token count (0 for an empty or
// all-whitespace line), or -1 if a quote is never closed. Never writes
// past `argv[max_args - 1]`; excess tokens beyond that are silently
// dropped, not overflowed into.
int shell_tokenize(char *line, char **argv, int max_args);

// Runs one already-tokenized command line (argc >= 1). Dispatches to a
// builtin (cd, exit, pwd) if argv[0] matches one, else forks and execvp's
// it. Returns the command's exit status: 0-255 on normal exit, 128+signal
// if killed by a signal, 127 if the command couldn't be found/executed.
// `exit` doesn't return -- it terminates the whole shell process directly,
// matching every real shell's behavior.
int shell_exec(int argc, char **argv);

// Reads and runs commands from `in` until EOF or a `exit` builtin.
// Prints "$ " to `out` before each read when `interactive` is non-zero
// (a real terminal); a piped/redirected stdin never gets a prompt, so
// script output stays exactly what the commands themselves produced.
// Returns the exit status of the last command run (0 if none was), which
// becomes the shell process's own exit code -- the same convention a
// real shell follows for a non-interactive run.
int shell_loop(FILE *in, FILE *out, int interactive);

#endif
