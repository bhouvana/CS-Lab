# Stack Frame Visualizer

**Linux/POSIX, x86-64 only.** Built and verified via WSL Ubuntu
(GCC 15.2.0, gdb 17.1, real `objdump`).

## What is this?

A tool that walks the **real** stack-frame chain of a running C
program — `main() -> foo() -> bar()` — using gdb as the introspection
engine, then renders it as a textual diagram. Every address in the
output comes from a live debugging session, not a fabricated example.

```bash
make build   # compiles nested.c, then walks its real stack frames via gdb
```

## Why does it matter?

"The stack" is usually explained with a hand-drawn diagram. This lab
draws the diagram from an actual process's actual memory instead —
RSP, RBP, saved return addresses, all pulled from a real debugger
session — so "stack frame" stops being a metaphor and becomes
something you can point a tool at and read out.

## Concept

```text
main() -> foo() -> bar()      (compiled with -fno-omit-frame-pointer)
         |
         v
       gdb                    break inside bar(), inspect all 3 live frames
         |
         v
  parsed frame data           function, args, frame base, saved RBP/RIP addresses
         |
         v
     STACK diagram
```

```text
STACK

bar()
 +-- local variables
 +-- saved RBP
 +-- return address

foo()
 +-- local variables
 +-- saved RBP
 +-- return address

main()
```

## How it works

- `src/nested.c` is compiled with `-O0 -fno-omit-frame-pointer` so RBP
  forms a real linked list of frames (each frame's saved-RBP slot
  points to the previous frame's base) — modern default builds often
  omit the frame pointer for a spare register, which would break this.
- `scripts/walk_stack.py` runs `gdb --batch -ex 'break bar' -ex run
  -ex 'info frame 0' -ex 'info frame 1' -ex 'info frame 2'`, capturing
  gdb's real introspection of the three live frames (bar, foo, main),
  then regex-parses that transcript into structured data and renders
  the diagram above with real addresses filled in.
- `scripts/frame_size_experiment.py` compiles small throwaway programs
  with varying numbers of local variables and reads their compiled
  frame size straight from `objdump` output — see Experiments.

## Implementation

- `src/nested.c` — the 3-level call chain being inspected.
- `scripts/walk_stack.py` — drives gdb, parses its output, renders the
  diagram.
- `scripts/frame_size_experiment.py` — the stack-frame-size experiment.
- `tests/test_walk_stack.py` — 5 tests: parser correctness against a
  frozen real gdb transcript (no gdb required to run these), an empty-
  input edge case, and one true end-to-end integration test that's
  skipped (not failed) when gdb isn't installed.

## Example

```text
$ make build
...
STACK

bar(x=15)  [frame base 0x7fffffffd830]
 +-- locals/args at 0x7fffffffd820
 +-- saved RBP stored at 0x7fffffffd820  (points to the caller's frame)
 +-- return address stored at 0x7fffffffd828  (-> 0x5555555551b9, in the caller)

foo(x=5)  [frame base 0x7fffffffd860]
 +-- locals/args at 0x7fffffffd850
 +-- saved RBP stored at 0x7fffffffd850  (points to the caller's frame)
 +-- return address stored at 0x7fffffffd858  (-> 0x5555555551f1, in the caller)

main()  [frame base 0x7fffffffd880]
 +-- locals/args at 0x7fffffffd870
 +-- saved RBP stored at 0x7fffffffd870  (points to the caller's frame)
 +-- return address stored at 0x7fffffffd878  (-> 0x7ffff7c2a601, in the caller)
```

`bar`'s saved return address (`0x...1b9`) is the exact instruction in
`foo` right after `call bar`; `foo`'s (`0x...1f1`) is the instruction
in `main` right after `call foo`. `main`'s return address lands inside
glibc's startup code (`__libc_start_main`), not user code — main() has
a caller too.

`make objdump` shows `bar`'s real prologue/epilogue:

```text
push   %rbp
mov    %rsp,%rbp
sub    $0x20,%rsp
...
leave
ret
```

## Experiments

**How does local variable count affect a function's compiled stack
frame size?** `scripts/frame_size_experiment.py` compiles a tiny
`noinline` function with 0, 1, 2, 4, 8, 16, and 32 `long` locals and
reads the frame size straight from its `sub $N,%rsp` prologue
instruction via `objdump`.

Real output from `make benchmark`:

```text
locals    bytes needed (locals*8)   actual frame (sub $N,%rsp)
0         0                         0
1         8                         0
2         16                        0
4         32                        0
8         64                        0
16        128                       8
32        256                       136
```

## Results

Up through 8 locals (64 bytes), the compiler emits **no `sub`
instruction at all** — confirmed by disassembling that case directly:
the function writes its locals to `-0x8(%rbp)` through `-0x40(%rbp)`
without ever touching RSP. This is the x86-64 System V ABI's **red
zone**: a leaf function (one that calls nothing else) is allowed to
use up to 128 bytes below RSP without reserving it, because nothing
else can clobber that memory. 16 locals (128 bytes) sits right at the
edge (a token `sub $0x8` appears, likely alignment bookkeeping); 32
locals (256 bytes, exceeding the red zone by 128 bytes) forces a real
`sub $0x88` (136 bytes — the 128-byte excess plus 8 bytes to keep RSP
16-byte aligned). This isn't a rounding quirk; it's the ABI's
red-zone optimization made directly visible.

## What I learned

My first read of this table looked like a bug (0 bytes allocated for
functions that clearly have real locals!) until I disassembled the
8-locals case directly and saw the `-0x40(%rbp)` through `-0x8(%rbp)`
addressing with no `sub` — that's when the red zone clicked as the
actual explanation rather than something being broken. `bar()` in
`src/nested.c`, by contrast, calls `printf`, so it's *not* a leaf
function and does emit a real `sub $0x20,%rsp` — the red zone only
applies when a function is certain nothing else will run on top of
that unreserved memory before it returns.

## Limitations

- x86-64 System V only (the red zone and RBP-chain conventions here
  don't carry over to, e.g., ARM64 or Windows x64, which has no red
  zone at all).
- Requires `-fno-omit-frame-pointer` — a default `-O2` build without
  it would give gdb (and this tool) a much harder time reconstructing
  frames, relying on DWARF CFI instead of a simple RBP chain.
- Depends on gdb's `info frame` text format, which could change
  between gdb versions (the parser was written against gdb 17.1's
  output).

## Further experiments

- Compile `bar()` itself with varying numbers of extra locals below
  the red zone threshold and confirm it also avoids a `sub` — right up
  until it needs to spill enough to exceed 128 bytes combined with the
  `printf` call's own requirements.
- Walk the frame chain past `main()` into `__libc_start_main` and
  identify what's really at the bottom of the call stack.
- Rebuild `nested.c` with `-O2` (frame pointer omitted by default) and
  see how much harder gdb's frame reconstruction becomes without it.
