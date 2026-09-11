# Shell

**Linux/POSIX only.** This lab is a small command-line shell in C. It
demonstrates the process and signal boundary behind an interactive shell:
`fork()`, `execvp()`, `waitpid()`, `chdir()`, pipes, and status handling.

```bash
make build
./shell
$ echo hello
hello
$ exit 0
```

## Why it matters

A shell turns a command line into processes. Builtins must run inside the
shell because they change shell state; external commands run in children so
the shell can remain alive and execute the next command. Pipelines add file
descriptor ownership and synchronization, while signals determine whether a
Ctrl-C kills the shell, the foreground command, or neither.

## Supported behavior

- `cd`, `pwd`, and `exit` are builtins. `cd` changes the shell's own working
  directory; `exit` accepts an optional numeric status.
- Other commands use `fork()` + `execvp()` + `waitpid()`.
- Double- and single-quoted text stays in one argument, so
  `echo "hello world"` produces two arguments rather than three.
- A two-stage or multi-stage pipeline uses `|`; the returned status is the
  final stage's status.
- `$?` expands to the previous command's status.
- `&&` runs the right command only after success; `||` runs it only after
  failure. Chains are evaluated left to right.
- Unknown commands return status `127`; other execution failures return
  `126`. A child terminated by signal `N` becomes status `128 + N`.
- The shell ignores `SIGINT` at its own prompt. A child restores the default
  `SIGINT` disposition before `execvp()`, so foreground commands remain
  interruptible.
- Prompts are printed only when stdin is a terminal. Piped scripts do not
  receive prompt noise.

## Design

`src/shell.c` is intentionally layered:

1. `shell_tokenize()` mutates a line in place, recognizes whitespace and
   matching quotes, and returns pointers into the original buffer.
2. `shell_exec_line()` expands `$?`, splits top-level `&&` and `||`, and
   delegates each runnable segment to the pipeline runner.
3. `shell_exec_pipeline()` splits unquoted `|` operators, creates all pipes
   before forking, connects each child with `dup2()`, closes unused
   descriptors, and waits for every child.
4. `shell_exec()` dispatches builtins in-process or runs one external child.
5. `shell_loop()` owns the prompt, input buffer, previous status, and final
   process exit status.

The tokenizer accepts at most `SHELL_MAX_ARGS - 1` arguments so there is
always room for a terminating null pointer. The line buffer is bounded by
`SHELL_MAX_LINE` (4096 bytes). Unterminated quotes return an error instead of
silently accepting a malformed command.

## Implementation

- `include/shell.h` documents the public API and fixed input limits.
- `src/shell.c` implements tokenization, builtins, process execution,
  pipelines, status operators, signal behavior, and the command loop.
- `src/main.c` detects whether stdin is a terminal and calls `shell_loop()`.
- `src/bench.c` drives the real `./shell` binary through a pipe and measures
  command dispatch, pipelines, status chains, and a raw `getpid` syscall.
- `tests/test_shell.c` contains 17 unit and integration tests. The
  integration tests fork and exec the real shell, feed scripts through pipes,
  check output and exit status, and include a SIGINT regression test.

## Tests

Run in Linux or WSL:

```bash
make test
make test-asan
```

The test suite covers whitespace, repeated delimiters, single and double
quotes, empty input, unterminated quotes, argument-limit behavior, builtins,
unknown commands, failed `cd`, EOF status, explicit exit status, pipelines,
`$?`, `&&`, `||`, and SIGINT survival.

## Benchmark results

Measured under WSL Ubuntu with the real shell binary. Each command-dispatch
row includes the shell's `fork()` + `execvp()` + `waitpid()` path.

```text
Benchmark: shell command-dispatch throughput (fork+exec+wait per command)

commands    elapsed(ms)   commands/sec
100         119.63        836
500         502.95        994
2000        1558.23       1284

raw getpid syscall: 108.57 ms (108.6 ns/call; 1000000 calls)
true | true: 199.80 ms for 200 pipelines
true && true: 299.07 ms for 200 chains
```

The raw syscall is not a replacement for a shell command: it is a control
measurement showing how much work process creation adds around one kernel
transition. The pipeline and `&&` timings include multiple command launches
and therefore measure orchestration overhead, not just operator parsing.

## What the results show

The shell processes roughly 0.8K to 1.3K trivial external commands per
second in this run. Throughput improves with a larger batch because startup
and measurement overhead are amortized, but every command still pays for a
new process image and a wait. A raw `getpid` syscall completes in about
109 ns, while a shell command is dominated by process creation and program
startup rather than by tokenization.

Pipelines add another child and pipe setup per stage. Status chains avoid
running commands when their short-circuit condition is false, but the
benchmark's `true && true` row still launches two commands for every chain.

## Failure and signal behavior

- A missing command is reported and returns `127`; the shell continues.
- `cd` reports the underlying `errno` message and returns status `1`.
- An unterminated quote returns status `2`.
- Empty or whitespace-only input is ignored.
- A foreground child receives normal default SIGINT behavior, while the
  shell process survives SIGINT at its prompt.
- Pipeline setup failures return a nonzero status and close descriptors
  owned by the parent.

## Limitations

- No redirection (`>`, `<`), background jobs (`&`), job control, command
  history, tab completion, globbing, environment-variable expansion, or
  shell scripting constructs such as `if` and `for`.
- Builtins inside a pipeline run in a child, so state changes such as `cd`
  cannot affect the parent shell. This matches the usual pipeline process
  model but is intentionally not a full Bash implementation.
- The parser is bounded by fixed line and argument limits and does not
  implement escape characters inside quotes.
- The implementation uses POSIX process, signal, and socket-adjacent APIs;
  it is not a native Windows program.
- `select()`/terminal behavior and process startup costs are operating-system
  dependent. Benchmark numbers are measurements of the recorded environment,
  not universal constants.

## Intentionally out of scope

This is a compact process-execution laboratory, not a Bash replacement.
Redirection, job control, shell scripting, and a larger parser each deserve
their own design and tests. Keeping them out preserves the central lesson:
how a small amount of C coordinates processes, file descriptors, exit
statuses, and signals.