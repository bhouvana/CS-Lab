# Tiny Language (AtlasLang)

## What is this?

AtlasLang: a deliberately tiny programming language, implemented as a
full pipeline — lexer, parser, AST, tree-walking interpreter — in Rust.

```bash
cargo run --bin atlas -- run    examples/hello.atlas
cargo run --bin atlas -- tokens examples/hello.atlas
cargo run --bin atlas -- ast    examples/hello.atlas
cargo run --release             # no args: pipeline demo on a bundled example
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

AtlasLang supports: integer literals, variables (`let`/reassignment),
arithmetic (`+ - * / %`), comparisons (`< <= > >= == !=`, yielding
0/1), `if`/`else`, `while`, and `print`. It deliberately has **no**
functions, classes, generics, modules, or type system — see
[CS-LAB.md](/CS-LAB.md) §9. The point is understanding language
implementation, not building a usable language.

## Implementation

- `src/lexer.rs` — hand-written scanner, one token at a time, `#` line
  comments, line-number tracking for error messages.
- `src/ast.rs` — `Expr`/`Stmt`/`BinOp` — the shape both the parser
  builds and the interpreter walks.
- `src/parser.rs` — recursive descent; expression precedence is
  comparison > addition > multiplication > unary > primary, each level
  its own function (classic precedence climbing).
- `src/interpreter.rs` — tree-walking evaluator over one flat
  `HashMap<String, i64>` (no block scoping — see Limitations).
- `src/lib.rs` — wires the pipeline together as `run_source()`.
- `src/main.rs` — CLI with three modes (`run`/`tokens`/`ast`) so every
  intermediate stage is directly inspectable.

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

## Experiments

This lab's "experiment" is the pipeline-visibility itself: run the same
program through `tokens`, `ast`, and `run` and see each stage's output
line up. Concretely, comparing `examples/max.atlas`'s AST above to its
source shows precedence climbing did its job — `a > b` parsed as a
single `Binary` node inside the `If`'s condition, not as three separate
statements.

## Results

10/10 tests pass, covering the full language surface: arithmetic
precedence and parens, reassignment, if/else (both branches), a while
loop that computes 8 Fibonacci numbers and is checked against the exact
expected sequence `[0,1,1,2,3,5,8,13]`, all six comparison operators,
and four invalid-input cases (division by zero, undefined variable,
assignment without `let`, and a syntax error) all producing the
expected error message prefix (`lex error` / `parse error` / `runtime
error`).

## What I learned

Distinguishing `Stmt::Let` (always inserts) from `Stmt::Assign` (checks
`contains_key` first) was the one place this interpreter almost had a
silent bug: without that check, `x = 5;` on an undeclared `x` would
have quietly created it, and a typo'd variable name would never be
caught. Making assignment-to-undeclared an explicit runtime error (and
testing for it) turned a footgun into a clear error message instead.

## Limitations

- One flat global variable scope — no block-scoped `let` inside `if`/
  `while` bodies (a `let` anywhere overwrites the same global slot).
- No functions, so no recursion — only iteration (`while`) is available.
- No strings, floats, arrays, or booleans as a first-class type;
  comparisons produce plain `i64` 0/1.
- Errors report a line number but not a column.

## Further experiments

- Add block scoping (a `Vec<HashMap<String,i64>>` scope stack) and
  measure how many more lines it costs.
- Add a `for` loop as sugar that desugars to an equivalent `while` in
  the parser, without touching the interpreter at all.
- Compile the AST to the stack-machine bytecode from
  `compilers/bytecode-vm` instead of tree-walking it directly.
