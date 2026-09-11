# Contributing

CS-Lab is a personal/educational laboratory, not a platform. Contributions
that fit its scope are welcome.

## Adding a new lab

1. Pick a fundamental CS idea not already covered.
2. Choose the language intentionally (see [CS-LAB.md](CS-LAB.md) §3) — don't
   default to whatever's convenient.
3. Follow the standard layout:
   ```
   <lab>/
   ├── README.md   (what/why/concept/how/example/experiments/results/limitations)
   ├── src/
   ├── tests/
   └── examples/
   ```
4. Keep it small. Target ~300 understandable lines over 3,000 abstracted ones.
5. Include at least one real experiment with real measured numbers — never
   fabricated benchmarks.
6. Include tests: normal case, edge case, invalid input, regression case.
7. Wire it into the root `Makefile` (Rust: `Cargo.toml` is enough, root
   Makefile finds it automatically) and add a row to the root README's lab
   table.

For Rust labs, the root Makefile's `benchmark` target runs `cargo run
--release` with no arguments — so the default binary should run the
lab's experiment when invoked with no subcommand (see
`algorithms/bloom-filter`'s `main.rs` for the convention).

## What not to add

No web UI, no auth, no databases-for-the-website, no cloud deployment, no
frameworks-for-their-own-sake, no AI features. See CS-LAB.md §1 and §45 for
the full list. If a lab can't be built and understood in about a day, its
scope is too big — shrink it.

## Code quality

- C/C++: build with `-Wall -Wextra -Wpedantic`; run sanitizers
  (`-fsanitize=address,undefined`) for memory-unsafe code.
- Rust: `cargo fmt`, `cargo clippy`, `cargo test`.
- Comments explain *why*, not *what*.

## Honesty

Never describe a lab as production-ready, industrial-grade, or secure unless
that's genuinely true. Use "educational implementation", "toy model",
"research prototype", "experimental simulator" instead.
