# Tiny Language (AtlasLang)

## What is this?

AtlasLang: a deliberately tiny programming language, implemented as a
full pipeline — lexer, parser, AST, tree-walking interpreter — in Rust.

```bash
cargo run --bin atlas -- run     examples/hello.atlas
cargo run --bin atlas -- tokens  examples/hello.atlas
cargo run --bin atlas -- ast     examples/hello.atlas
cargo run --bin atlas -- compile examples/hello.atlas   # emit bytecode-vm assembly instead of running it
cargo run --release              # no args: pipeline demo on a bundled example
```

## Why does it matter?

Every language, from a config DSL to a full compiler, goes through this
same shape. Seeing all four stages in ~350 lines of readable Rust — with
CLI switches to inspect each intermediate representation — makes "how do
programming languages work?" concrete instead of theoretical.

## Concept

```text
source
  |
  v
lexer      (characters -> tokens, tracked by line for error messages)
  |
  v
parser     (tokens -> AST, recursive descent with precedence climbing)
  |
  v
AST        (Stmt/Expr trees)
  |
  v
interpreter (tree-walking evaluation over a flat variable environment)
```

AtlasLang supports: integer literals, variables (`let`/reassignment,
block-scoped), arithmetic (`+ - * / %`), comparisons (`< <= > >= == !=`,
yielding 0/1), `if`/`else`, `while`, `for` (sugar for `while`, see
below), and `print`. It deliberately has **no** functions, classes,
generics, modules, or type system — see [CS-LAB.md](/CS-LAB.md) §9. The
point is understanding language implementation, not building a usable
language.

**Block scoping**: every `if`/`while`/`for` body is its own child scope.
A `let` inside a block declares into that block only — it's gone once
the block exits, and it *shadows* (doesn't overwrite) a same-named
variable from an outer scope for the block's duration. A plain
assignment (`x = ...`, no `let`) searches outward through enclosing
scopes and mutates whichever one already has the name, so a loop body
can still update a variable declared before the loop.

**`for` loops** are pure syntactic sugar, added entirely in the parser:
`for (let i = 0; i < n; i = i + 1) { body }` desugars to
`{ let i = 0; while (i < n) { body; i = i + 1; } }` — a `Stmt::Block`
wrapping a `Let` and a `While`. The interpreter has no `for`-specific
code at all; it only ever sees the `Block`/`While`/`Assign` nodes it
already knew how to run.

## Implementation

- `src/lexer.rs` — hand-written scanner, one token at a time, `#` line
  comments, line-number tracking for error messages.
- `src/ast.rs` — `Expr`/`Stmt`/`BinOp` — the shape both the parser
  builds and the interpreter walks.
- `src/parser.rs` — recursive descent; expression precedence is
  comparison > addition > multiplication > unary > primary, each level
  its own function (classic precedence climbing).
- `src/interpreter.rs` — tree-walking evaluator over a
  `Vec<HashMap<String, i64>>` scope stack: `let` declares into the
  innermost scope, reads/assignment search outward.
- `src/compiler.rs` — an alternative to tree-walking: compiles the AST
  to `compilers/bytecode-vm`'s text assembly format instead (see
  Experiments).
- `src/lib.rs` — wires the pipeline together as `run_source()` and
  `compile_source()`.
- `src/main.rs` — CLI with four modes (`run`/`tokens`/`ast`/`compile`)
  so every intermediate stage, including the alternative backend, is
  directly inspectable.

## Example

```text
$ cat examples/fib.atlas
let a = 0;
let b = 1;
let i = 0;
while (i < 10) {
    print(a);
    let t = a + b;
    a = b;
    b = t;
    i = i + 1;
}

$ cargo run --bin atlas -- run examples/fib.atlas
0
1
1
2
3
5
8
13
21
34

$ cargo run --bin atlas -- tokens examples/hello.atlas
   1: Let
   1: Ident("x")
   1: Assign
   1: Int(10)
   1: Semi
   ...
   4: Eof

$ cargo run --bin atlas -- ast examples/max.atlas
Let("a", Int(7))
Let("b", Int(42))
If(Binary(Var("a"), Gt, Var("b")), [Print(Var("a"))], [Print(Var("b"))])
```

(the `ast` output above is condensed; the real CLI pretty-prints it
across multiple lines via `{:#?}`.)

```text
$ cargo run --bin atlas -- compile examples/fib.atlas
PUSH 0
STORE 0
PUSH 1
STORE 1
PUSH 0
STORE 2
LOAD 2
PUSH 10
LT
JUMP_IF_FALSE 25
LOAD 0
PRINT
...
JUMP 6
HALT
```

## Experiments

This lab's first "experiment" is the pipeline-visibility itself: run the
same program through `tokens`, `ast`, and `run` and see each stage's
output line up. Concretely, comparing `examples/max.atlas`'s AST above
to its source shows precedence climbing did its job — `a > b` parsed as
a single `Binary` node inside the `If`'s condition, not as three
separate statements.

**Does compiling to bytecode instead of tree-walking actually run
faster, and does it produce identical results?** `src/compiler.rs`
compiles the same AST to `compilers/bytecode-vm`'s bytecode instead of
walking it. That VM's ISA had no comparison or modulo opcodes (only
PUSH/POP/ADD/SUB/MUL/DIV/LOAD/STORE/JUMP/JUMP_IF_FALSE/PRINT/HALT) --
`LT`/`LE`/`GT`/`GE`/`EQ`/`NE`/`MOD` were added to that lab specifically
to make this compiler possible (same pop-two/push-result shape as the
existing arithmetic opcodes; see `compilers/bytecode-vm`'s own
changelog).

Correctness, first: the existing 8-Fibonacci-number example, compiled
and run on the real `vm` binary via `tests/tests.rs`'s
`compiled_bytecode_runs_on_the_real_vm_and_matches_the_interpreter_normal_case`,
against the tree-walking interpreter on the same source:

```text
interpreter: 0 1 1 2 3 5 8 13
vm (compiled): 0 1 1 2 3 5 8 13   -- byte-for-byte identical
```

Speed, on a 1,000,000-iteration summing loop (`let sum = 0; let i = 0;
while (i < 1000000) { sum = sum + i; i = i + 1; } print(sum);`), timed
with `time` on the same machine (interpreter: native Windows release
build; VM: WSL Ubuntu gcc -O2 build -- this repo's two toolchains, see
`docs/reproducibility.md`, so not a perfectly matched compiler-flags
comparison, but the same physical hardware):

```text
tree-walking interpreter:  0.570s
compiled, run on the VM:   0.087s
-> ~6.5x faster, both produce 499999500000
```

## Results

17 tests pass covering language behavior (arithmetic precedence,
reassignment, if/else, comparisons, invalid-input cases) plus block
scoping and `for`-loop desugaring specifically: a `let` inside a block
doesn't leak outward, shadows an outer variable of the same name for
the block's duration and then restores it, a plain assignment still
finds and mutates an *outer* variable from inside a block, and a `for`
loop's own init variable is scoped to the loop. Plus 3 for the
bytecode compiler: well-formed output, undefined-variable rejection at
compile time, and the interpreter-vs-VM equivalence check above.

The ~6.5x speedup is real but the *reason* is more specific than
"bytecode is faster than tree-walking" in general: this interpreter's
every variable read/write does a `HashMap<String, i64>` lookup (string
hashing + comparison) through however many scopes are on the stack,
while the VM does an array index into a fixed 256-slot `int64_t[]` --
the gap is dominated by that one difference, not by AST-node dispatch
overhead, which is a much smaller cost for a language this small.

## What I learned

Distinguishing `Stmt::Let` (always inserts) from `Stmt::Assign` (checks
`contains_key` first) was the one place this interpreter almost had a
silent bug: without that check, `x = 5;` on an undeclared `x` would
have quietly created it, and a typo'd variable name would never be
caught. Making assignment-to-undeclared an explicit runtime error (and
testing for it) turned a footgun into a clear error message instead.

Writing the bytecode compiler surfaced a real gap in a *different*
lab: `compilers/bytecode-vm`'s ISA couldn't express a single `if`
condition, because it had no comparison opcodes at all -- a limitation
invisible until something actually tried to target that ISA from a
higher-level language, exactly the kind of gap a from-scratch
implementation doesn't discover until it's stressed by a real use case.

## Limitations

- No functions, so no recursion — only iteration (`while`/`for`) is
  available.
- No strings, floats, arrays, or booleans as a first-class type;
  comparisons produce plain `i64` 0/1.
- Errors report a line number but not a column.
- The bytecode compiler does **not** replicate the interpreter's block
  scoping: the VM's memory is one flat 256-slot array with no scope
  stack, so it gives every distinct variable *name* one fixed slot on
  first `let`. Two different block-scoped variables sharing a name
  (shadowing) would collide onto the same slot when compiled — the
  compiler is correct for straight-line and loop-based programs
  without shadowing (everything in this lab's own examples and tests),
  not a full reimplementation of the interpreter's scoping semantics.
- The interpreter-vs-VM equivalence test needs a `compilers/bytecode-vm`
  binary already built next to this crate; it's a real, meaningful
  check where both toolchains are available (this repo's CI is), and a
  clean skip (not a failure) where they aren't, e.g. `cargo test` on
  native Windows with no C compiler on `PATH`.
