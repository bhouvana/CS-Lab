# Syscall Lab

**Linux x86-64 only.** Built and verified via WSL Ubuntu (GCC 15.2.0,
`strace` 6.19). This is genuinely Linux-specific — the `syscall`
instruction, syscall numbers, and calling convention here have no
meaning on Windows or macOS.

## What is this?

Direct Linux system calls, hand-written in assembly: `read`, `write`,
`getpid`, and
`exit`, without going through libc's `write()`/`exit()` at any point.
Includes a complete `-nostdlib` program whose entry point is a raw
`_start` written in assembly — no libc, no C runtime, at all.

```bash
make build
 73.19    0.060438          30      2001           write
 26.81    0.022142          22      1000           getpid
------ ----------- ----------- --------- --------- ----------------
100.00    0.082580          27      3001           total
```

This trace was collected with `SYSCALL_BENCH_ITERATIONS=1000` on the
WSL Ubuntu environment named at the top of this file. The extra write
is the benchmark's final buffered `printf` output; the syscall calls
themselves account for 1,000 `getpid` calls and 2,000 one-byte writes.
The exact timing is kernel/filesystem dependent; run
`make strace-benchmark` for a fresh measurement.
kernel. Seeing that path with nothing else in between — no libc
buffering, no wrapper doing errno translation — makes "user space" and
"kernel space" into two sides of one visible instruction instead of an
abstract phrase.

## Concept

```text
user program
      |
      v
register arguments     RAX=syscall number, RDI/RSI/RDX/R10/R8/R9=args
      |
      v
   syscall              traps into the kernel
      |
      v
    kernel               performs the actual write/exit
      |
      v
 return value (RAX)      >=0 on success, -errno on failure (raw, not
                          libc's "-1, check the errno global")
```

## How it works

- **`src/syscalls.S`**: `my_read`/`my_write`/`my_getpid`/`my_exit`, tiny
  wrappers. Since `read` and `write`'s first 3 arguments and the C calling convention's first 3
  argument registers happen to coincide (RDI, RSI, RDX), these barely
  move anything — load the syscall number into RAX, `syscall`, `ret`.
- **`src/cat.c`**: a byte-for-byte stdin-to-stdout copier using only
  `my_read`, `my_write`, and `my_exit` for I/O and termination.
- **`src/hello_nolibc.S`**: a complete program with **no libc and no C
  runtime at all**. `_start` (not `main`) is the literal entry point
  the kernel jumps to; built with `-nostdlib -static` so nothing else
  gets linked in.
- **Raw error convention**: a failed syscall returns `-errno` directly
  in RAX (e.g. `-9` for `EBADF`). Libc's `write()` is what turns that
  into "return -1, and stash 9 in the `errno` global" — bypassing libc
  means bypassing that translation too, which `tests/test_syscalls.c`
  checks for directly.

## Implementation

- `src/syscalls.S` — the syscall wrappers.
- `src/hello_nolibc.S` — the standalone, libc-free program.
- `src/main.c` — a C program driving the wrappers.
- `src/cat.c` — the raw-syscall byte copier.
- `src/bench.c` — the syscall-workload experiment.
- `tests/test_syscalls.c` — 6 tests: reading from a pipe, writing to a real file (verified
  by reading it back), zero-byte write, invalid fd (raw `-EBADF`), a
  closed fd, and `my_exit`'s real exit status via `fork`+`waitpid`
  (since calling it directly would end the test process).

## Example

```text
$ ./hello_nolibc
Hello from a raw Linux syscall -- no libc involved.

$ ./demo
printed via my_write(), a hand-written syscall wrapper
my_write returned 55 (bytes written)
my_write(-1, ...) returned -9 (raw kernel -EBADF, not libc's -1+errno)
about to call my_exit(7) -- process should exit with status 7

$ echo $?
7
```

`make objdump` shows the entire compiled body of the wrappers:

```text
0000000000001219 <my_read>:
  1219:\tmov    $0x0,%rax
  1220:\tsyscall
  1222:\tret

0000000000001223 <my_write>:
  1223:\tmov    $0x1,%rax
  122a:\tsyscall
  122c:\tret

000000000000122d <my_exit>:
  122d:\tmov    $0x3c,%rax
  1234:\tsyscall
```

The read, write, and getpid wrappers are three instructions each; exit
is two because it never returns. That's the entire "user program ->
kernel" path this lab set out to make visible.

## Experiments

**How much does crossing into the kernel cost, and how much does the
kernel work matter?** `src/bench.c` runs 1 million iterations each of
`my_getpid`, `my_write` to `/dev/null`, `my_write` to a real file, and a
trivial no-syscall function call. The real-file loop writes one million
bytes and deletes the temporary file before exiting.

Real output from `make benchmark`:

```text
Benchmark: syscall cost by kernel workload (1000000 iterations)

my_getpid (no arguments):    112.74 ms  (112.7 ns/call)
my_write(devnull, ...):    156.14 ms  (156.1 ns/call)
my_write(real file, ...):  9420.28 ms  (9420.3 ns/call)
noop_call (no syscall):      0.30 ms  (0.3 ns/call)
getpid/write(devnull) ratio: 0.72x
write(real file)/write(devnull) ratio: 60.33x
write(devnull)/noop ratio: 512x
sink (ignore): 1000000
```

## Results

On this run, `getpid` was **112.7ns** and a one-byte `/dev/null` write
was **156.1ns**, while the one-byte real-file write was **9420.3ns**.
The real file was **60.33x** slower than `/dev/null`, showing that the
fixed kernel-entry cost is only part of the total. The `/dev/null` write
was **512x** the plain call baseline. These timings are one run on the
WSL Ubuntu environment named at the top of this file, not universal
constants.

The short `make strace-benchmark` target repeats the same benchmark with
`SYSCALL_BENCH_ITERATIONS=1000`, so `strace -c` can finish without
tracing a million disk writes:

```text
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 56.75    0.056670          28      2001           write
 43.25    0.043186          43      1000           getpid
------ ----------- ----------- --------- --------- ----------------
100.00    0.099856          33      3001           total
```

This trace was collected with `SYSCALL_BENCH_ITERATIONS=1000` on the
WSL Ubuntu environment named at the top of this file. The extra write
is the benchmark's final buffered `printf` output; the syscall calls
themselves account for 1,000 `getpid` calls and 2,000 one-byte writes.
The exact timing is kernel/filesystem dependent; run
`make strace-benchmark` for a fresh measurement.

## What I learned

`./demo`'s output was silently missing three of its four `printf`
lines the first time I ran it — only the line printed via the raw
`my_write` syscall showed up. `strace` revealed why: the process really
did reach `my_exit(7)` and really did exit with status 7 (both fully
correct), but `my_exit` is a raw `syscall` instruction, not libc's
`exit()` — it never flushes libc's buffered stdio. Every `printf` line
was sitting in a buffer that nothing ever wrote out. Adding one
`fflush(stdout)` before the `my_exit(7)` call fixed the demo, but the
real lesson survived the fix: "bypassing libc" isn't just about the
syscall you're demonstrating, it's about *everything else* libc was
quietly doing for you that you now have to do yourself.

## Limitations

- Only `read`, `write`, `getpid`, and `exit` — no `open` or `mmap`, though the
  same three-instruction pattern extends directly to other syscalls that
  needs no argument marshaling.
- No `vDSO` discussion — some syscalls (`gettimeofday`, `clock_gettime`)
  are actually served from userspace via the vDSO for speed, which
  this lab's straightforward `syscall`-instruction model doesn't cover.
- x86-64 only; ARM64 Linux uses a different instruction (`svc #0`) and
  different syscall numbers entirely.
