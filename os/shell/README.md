# Shell

## What is this?

A small POSIX command-line shell in C: read a line, split it into
arguments, run it (`fork()` + `execvp()` + `waitpid()`, or a builtin),
repeat. `cd`, `exit`, and `pwd` are builtins; everything else is looked
up on `PATH` and run as a child process.

```bash
./shell
$ echo hello
hello
$ exit 0
```

## Why does it matter?

A shell is where "the OS" stops being an abstraction and becomes three
concrete syscalls in a loop: `fork()` to get a new process, `execvp()`
to replace its image with the program you asked for, `waitpid()` to
find out how it went. Everything a shell *feels* like it does —
running commands, reporting failures, surviving Ctrl-C — is built on
just those three calls plus careful handling of what happens when they
don't go as planned: a command that doesn't exist, a directory that
isn't there, a signal arriving mid-prompt.

## Experiments

The three follow-up experiments are implemented in the shell and its
existing benchmark harness:

1. **Pipelines.** `shell_exec_line()` splits top-level `|` operators,
  forks one process per stage, connects adjacent pipes, and reports the
  final stage's status. `make benchmark` compares 200 `true` commands
  with 200 `true | true` pipelines.
2. **Process creation versus one syscall.** The benchmark compares its
  `true` command baseline with a Linux x86-64 raw `getpid` syscall in
  `src/bench.c`. This isolates the cost of the shell's fork/exec/wait
  round trip from a single kernel transition, using the same raw syscall
  model as `assembly/syscall-demo` without changing that unrelated lab.
3. **Status operators.** `$?` expands to the previous command's status;
  `&&` runs its right side only after success and `||` runs it only after
  failure. The integration test covers both short-circuit paths and the
  expanded status value.

## Results

The source and editor diagnostics validate cleanly, but executable
measurements are blocked in this Windows workspace. The available GCC is
MinGW and the direct shell build fails at the first POSIX-only include:

```text
src/shell.c:12:10: fatal error: sys/wait.h: No such file or directory
```

Therefore `build`, `test`, `benchmark`, and `test-asan` have no runnable
result here. The Makefile already documents the intended Linux/POSIX
environment and the sanitizer target should be run there; no timing
numbers are recorded as if they were measured on this host.
  if it contains spaces — `echo "hello world"` is 2 tokens, not 3. An
  unterminated quote is rejected (`-1`), not silently truncated.
- **Builtins run in the shell's own process** because they change the
  shell's own state: `cd` calls `chdir()` directly (a child process's
  `chdir()` would only change *its own* copy of the working directory,
  which vanishes the moment it exits — this has to happen in-process to
  do anything at all), `exit` calls the real `exit()`, `pwd` reads
  `getcwd()`.
- **Everything else is `fork()` + `execvp()` + `waitpid()`.** The child
  resets `SIGINT` to its default disposition right after `fork()` (see
  below) and then `execvp()`s; if that fails (unknown command), it
  prints an error and exits `127` — the same convention every real
  shell uses for "command not found" — rather than crashing.
- **Exit-status convention** matches a real shell: a normal exit
  returns that code (0-255); a child killed by a signal returns
  `128 + signal number`; the shell process's own exit status (what
  running `./shell; echo $?` from a *real* shell would show) is
  whatever the last command inside it returned.
- **`SIGINT` handling**: the shell process itself calls
  `signal(SIGINT, SIG_IGN)` at startup, so pressing Ctrl-C at the `$ `
  prompt never kills the shell. A child resets `SIGINT` back to
  `SIG_DFL` immediately after `fork()`, so a *running command* still
  dies normally to Ctrl-C — the shell is only immune while it's the one
  waiting at the prompt.
- **Interactive vs. piped**: `main()` checks `isatty(fileno(stdin))` and
  only prints the `$ ` prompt when stdin is a real terminal, so piping a
  script into `./shell` produces exactly the commands' own output, no
  prompt noise mixed in.

## Implementation

- `include/shell.h` — the public shell API.
- `src/shell.c` — tokenizing, the 3 builtins, `fork`/`exec`/`wait`, the
  read-eval loop.
- `src/main.c` — the entry point (12 lines).
- `src/bench.c` — the command-throughput and follow-up experiments.
- `tests/test_shell.c` — 17 tests: 9 direct unit tests against
  `shell_tokenize()` (normal cases, quoting, edge cases, 2 invalid
  unterminated-quote cases, an over-`max_args` case), and 8 integration
  tests that `fork()`+`exec()` the real `./shell` binary and drive it
  over a pipe (same pattern as `networking/tcp-chat`'s tests) —
  including a named regression test that sends the shell a real
  `SIGINT` and confirms it's still alive and working afterward, plus
  pipeline and `&&`/`||`/`$?` integration cases.

## Example

```text
$ pwd
/home/user/cs-lab/os/shell
$ cd /tmp
$ pwd
/tmp
$ cd /no/such/place
shell: cd: /no/such/place: No such file or directory
$ this_does_not_exist
shell: this_does_not_exist: No such file or directory
$ exit 3
```

(`echo $?` from the real shell that ran `./shell` confirms exit code 3.)

## Experiments

The three follow-up experiments are implemented in the shell and its
existing benchmark harness:

1. **Pipelines.** `shell_exec_line()` splits top-level `|` operators,
  forks one process per stage, connects adjacent pipes, and reports the
  final stage's status. `make benchmark` compares 200 `true` commands
  with 200 `true | true` pipelines.
2. **Process creation versus one syscall.** The benchmark compares its
  `true` command baseline with a Linux x86-64 raw `getpid` syscall in
  `src/bench.c`. This isolates the cost of the shell's fork/exec/wait
  round trip from a single kernel transition, using the same raw syscall
  model as `assembly/syscall-demo` without changing that unrelated lab.
3. **Status operators.** `$?` expands to the previous command's status;
  `&&` runs its right side only after success and `||` runs it only after
  failure. The integration test covers both short-circuit paths and the
  expanded status value.

## Results

The source and editor diagnostics validate cleanly, but executable
measurements are blocked in this Windows workspace. The available GCC is
MinGW and the direct shell build fails at the first POSIX-only include:

```text
src/shell.c:12:10: fatal error: sys/wait.h: No such file or directory
```

Therefore `build`, `test`, `benchmark`, and `test-asan` have no runnable
result here. The Makefile already documents the intended Linux/POSIX
environment and the sanitizer target should be run there; no timing
numbers are recorded as if they were measured on this host.

## What I learned

The benchmark's own output got duplicated and grew with every trial
the first time this ran — the same class of bug already documented in
`networking/tcp-chat`'s `bench.c`: `fork()` copies the parent's
still-buffered (unflushed) `stdout` into the child, and something in
the child's lifecycle (here, `freopen()`'s implicit close-and-flush of
the old stream) writes that inherited buffer out a second time, into
the descriptor the child still shared with the parent. Adding
`fflush(stdout)` immediately before every `fork()` — the exact fix
tcp-chat's benchmark already uses — fixed it.

## Limitations

- No redirection (`>`, `<`) or background jobs (`&`) — see "What this
  intentionally does NOT do" below.
- No shell scripting: no variables, no `if`/`for`. `exit` takes a
  literal numeric argument, not an expression.
- No job control (`fg`/`bg`/`jobs`) — a single foreground child at a
  time is the whole model.
- `SIGINT` is the only signal handled specially. `SIGTSTP` (Ctrl-Z) and
  others use their default disposition (which, for the shell process,
  means it isn't stoppable independently of its terminal session).
- `-fsanitize=address,undefined` (CS-LAB.md §40) is the separate
  `make test-asan` target, not the default `make test` — this repo's
  MinGW/Windows toolchain has no ASan/UBSan runtime; run `test-asan` on
  Linux. This lab is Linux/POSIX-only regardless (`fork`/`execvp`
  aren't available on Windows), same as `networking/tcp-chat`.

## What this intentionally does NOT do

This is a shell lab, not an attempt to reimplement Bash. On purpose,
it never grows: I/O redirection, globbing/wildcard
expansion, environment-variable expansion (`$HOME` typed literally
does nothing special), command history, tab completion, or a
configuration file. Each of those is a real, separate feature with its
own design space (redirection alone needs descriptor parsing and careful
close ordering) — bolting them on here would trade "a shell small enough
to read start to finish" for
"a worse Bash," which is exactly the tradeoff CS-LAB.md's scope rules
warn against.
