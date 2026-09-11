# Syscall Lab

**Linux x86-64 only.** Built and verified via WSL Ubuntu (GCC 15.2.0,
`strace` 6.19). This is genuinely Linux-specific — the `syscall`
instruction, syscall numbers, and calling convention here have no
meaning on Windows or macOS.

## What is this?

Direct Linux system calls, hand-written in assembly: `write` and
`exit`, without going through libc's `write()`/`exit()` at any point.
Includes a complete `-nostdlib` program whose entry point is a raw
`_start` written in assembly — no libc, no C runtime, at all.

```bash
make build
./hello_nolibc   # zero libc, zero C runtime — a bare ELF entry point
./demo           # C program using the hand-written syscall wrappers
```

## Why does it matter?

Every "hello world" goes through `write()`, and `write()` eventually
becomes exactly this: a magic number in RAX, arguments in a few other
registers, and a `syscall` instruction that hands control to the
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

- **`src/syscalls.S`**: `my_write`/`my_exit`, two tiny wrappers. Since
  `write`'s first 3 arguments and the C calling convention's first 3
  argument registers happen to coincide (RDI, RSI, RDX), these barely
  move anything — load the syscall number into RAX, `syscall`, `ret`.
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
- `src/bench.c` — the syscall-overhead experiment.
- `tests/test_syscalls.c` — 5 tests: writing to a real file (verified
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

`make objdump` shows the entire compiled body of both wrappers:

```text
0000000000001219 <my_write>:
    1219:	mov    $0x1,%rax
    1220:	syscall
    1222:	ret

0000000000001223 <my_exit>:
    1223:	mov    $0x3c,%rax
    122a:	syscall
```

Three and two instructions. That's the entire "user program -> kernel"
path this lab set out to make visible.

## Experiments

**How much does crossing into the kernel actually cost, compared to a
plain userspace call?** `src/bench.c` runs 1 million iterations of
`my_write` to `/dev/null` against 1 million iterations of a trivial
no-syscall function call.

Real output from `make benchmark`:

```text
my_write(devnull, ...):    130.75 ms  (130.8 ns/call)
noop_call (no syscall):      0.24 ms  (0.2 ns/call)
ratio: syscall is 534x the cost of a plain call
```

## Results

A syscall costs **~534x** a plain userspace function call in this
measurement — about 131ns vs. 0.2ns. That's the real, measurable cost
of a context switch into the kernel and back (privilege level change,
kernel-side validation of the file descriptor and buffer, and the
return trip), even for `/dev/null`, the cheapest possible destination.
This is exactly why libc's `stdio` buffers output instead of calling
`write()` per byte or per line — batching amortizes this fixed cost
over many bytes.

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

- Only `write` and `exit` — no `read`, `open`, or `mmap`, though the
  same three-instruction pattern extends directly to any syscall that
  needs no argument marshaling.
- No `vDSO` discussion — some syscalls (`gettimeofday`, `clock_gettime`)
  are actually served from userspace via the vDSO for speed, which
  this lab's straightforward `syscall`-instruction model doesn't cover.
- x86-64 only; ARM64 Linux uses a different instruction (`svc #0`) and
  different syscall numbers entirely.

## Further experiments

- Add `my_read` and build a byte-for-byte `cat` using only the three
  wrappers, no libc I/O at all.
- Measure syscall overhead across different syscalls (`getpid` — no
  arguments, minimal kernel work — vs. `write` to a real file) to see
  how much of the 534x is the context switch itself vs. actual kernel
  work.
- Compare this repo's raw-syscall numbers against `strace -c`'s own
  per-syscall timing on the same workload.
