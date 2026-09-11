use std::path::Path;
use std::process::Command;

use tiny_language::{compile_source, run_source};

#[test]
fn arithmetic_and_print_normal_case() {
    let out = run_source("let x = 10; let y = 20; print(x + y);").unwrap();
    assert_eq!(out, vec![30]);
}

#[test]
fn variable_reassignment() {
    let out = run_source("let x = 1; x = x + 1; x = x + 1; print(x);").unwrap();
    assert_eq!(out, vec![3]);
}

#[test]
fn if_else_both_branches() {
    let taken = run_source("if (1) { print(1); } else { print(2); }").unwrap();
    assert_eq!(taken, vec![1]);
    let not_taken = run_source("if (0) { print(1); } else { print(2); }").unwrap();
    assert_eq!(not_taken, vec![2]);
}

#[test]
fn while_loop_fibonacci() {
    let src = "let a = 0; let b = 1; let i = 0; \
               while (i < 8) { print(a); let t = a + b; a = b; b = t; i = i + 1; }";
    let out = run_source(src).unwrap();
    assert_eq!(out, vec![0, 1, 1, 2, 3, 5, 8, 13]);
}

#[test]
fn comparison_operators_yield_zero_or_one() {
    let out = run_source("print(3 < 5); print(5 < 3); print(3 == 3); print(3 != 3);").unwrap();
    assert_eq!(out, vec![1, 0, 1, 0]);
}

#[test]
fn operator_precedence_and_parens() {
    // 2 + 3 * 4 = 14, (2 + 3) * 4 = 20
    let out = run_source("print(2 + 3 * 4); print((2 + 3) * 4);").unwrap();
    assert_eq!(out, vec![14, 20]);
}

#[test]
fn division_by_zero_is_a_runtime_error_invalid_case() {
    let err = run_source("print(1 / 0);").unwrap_err();
    assert!(err.contains("division by zero"), "unexpected error: {err}");
}

#[test]
fn undefined_variable_is_a_runtime_error_invalid_case() {
    let err = run_source("print(missing);").unwrap_err();
    assert!(err.contains("undefined variable"), "unexpected error: {err}");
}

#[test]
fn assign_without_let_is_rejected_invalid_case() {
    let err = run_source("x = 5;").unwrap_err();
    assert!(err.contains("undeclared variable"), "unexpected error: {err}");
}

#[test]
fn syntax_error_is_reported_invalid_case() {
    let err = run_source("let x = ;").unwrap_err();
    assert!(err.starts_with("parse error"), "unexpected error: {err}");
}

#[test]
fn block_scoped_let_does_not_leak_outward_normal_case() {
    // `y` is declared only inside the if-block; using it afterward must
    // fail exactly like any other undefined variable would.
    let err = run_source("if (1) { let y = 5; } print(y);").unwrap_err();
    assert!(err.contains("undefined variable"), "unexpected error: {err}");
}

#[test]
fn block_scoped_let_shadows_and_then_restores_outer_normal_case() {
    // Inner `x` shadows the outer one for the block's duration; once the
    // block exits, the outer `x` is unchanged -- real lexical scoping,
    // not "the same variable, temporarily overwritten."
    let out = run_source("let x = 1; if (1) { let x = 99; print(x); } print(x);").unwrap();
    assert_eq!(out, vec![99, 1]);
}

#[test]
fn assign_inside_a_block_mutates_the_outer_variable_normal_case() {
    // Contrast with the shadowing test above: `x = ...` (no `let`)
    // finds and mutates the *outer* x, since a plain assignment
    // searches outward rather than declaring a new shadow.
    let out = run_source("let x = 1; if (1) { x = 99; } print(x);").unwrap();
    assert_eq!(out, vec![99]);
}

#[test]
fn while_loop_local_let_resets_every_iteration_regression() {
    // Guards the block-scoping refactor's behavior for the pattern the
    // existing Fibonacci test already relies on: a `let` inside a loop
    // body is redeclared (fresh scope) every iteration, not left over
    // from the previous one.
    let out = run_source("let i = 0; while (i < 3) { let doubled = i * 2; print(doubled); i = i + 1; }").unwrap();
    assert_eq!(out, vec![0, 2, 4]);
}

#[test]
fn for_loop_counts_like_the_equivalent_while_loop_normal_case() {
    let for_out = run_source("for (let i = 0; i < 5; i = i + 1) { print(i); }").unwrap();
    let while_out = run_source("let i = 0; while (i < 5) { print(i); i = i + 1; }").unwrap();
    assert_eq!(for_out, while_out);
    assert_eq!(for_out, vec![0, 1, 2, 3, 4]);
}

#[test]
fn for_loop_init_variable_is_scoped_to_the_loop_normal_case() {
    // The desugared form wraps init+while in a Block specifically so
    // `i` doesn't leak past the loop -- verify that's actually true.
    let err = run_source("for (let i = 0; i < 3; i = i + 1) { print(i); } print(i);").unwrap_err();
    assert!(err.contains("undefined variable"), "unexpected error: {err}");
}

#[test]
fn for_loop_body_can_still_mutate_an_outer_variable_normal_case() {
    let out = run_source("let sum = 0; for (let i = 0; i < 5; i = i + 1) { sum = sum + i; } print(sum);").unwrap();
    assert_eq!(out, vec![10]); // 0+1+2+3+4
}

#[test]
fn compile_source_produces_well_formed_bytecode_normal_case() {
    let asm = compile_source("let x = 1; let y = 2; print(x + y);").unwrap();
    // Every non-empty line is a real bytecode-vm mnemonic, and it ends
    // in HALT -- a cheap sanity check independent of the full VM
    // execution test below (which needs a built VM binary present).
    let lines: Vec<&str> = asm.lines().collect();
    assert_eq!(lines.last(), Some(&"HALT"));
    for line in &lines {
        let mnemonic = line.split_whitespace().next().unwrap();
        assert!(
            [
                "PUSH",
                "ADD",
                "SUB",
                "MUL",
                "DIV",
                "MOD",
                "LT",
                "LE",
                "GT",
                "GE",
                "EQ",
                "NE",
                "LOAD",
                "STORE",
                "JUMP",
                "JUMP_IF_FALSE",
                "PRINT",
                "HALT"
            ]
            .contains(&mnemonic),
            "not a real bytecode-vm mnemonic: {line}"
        );
    }
}

#[test]
fn compile_source_rejects_undefined_variable_invalid_case() {
    let err = compile_source("print(missing);").unwrap_err();
    assert!(err.contains("undefined variable"), "unexpected error: {err}");
}

/// Finds a compilers/bytecode-vm binary already built next to this
/// crate, trying both the Linux/WSL name and the native-Windows/MinGW
/// one. Returns None (not a panic) when neither exists -- this repo's
/// two toolchains live on different sides of the same machine (see
/// docs/reproducibility.md), so a Rust `cargo test` run on native
/// Windows has no C compiler to build the VM with, and shouldn't fail
/// over that.
fn find_vm_binary() -> Option<std::path::PathBuf> {
    for candidate in ["../bytecode-vm/vm", "../bytecode-vm/vm.exe"] {
        let p = Path::new(candidate);
        if p.exists() {
            return Some(p.to_path_buf());
        }
    }
    None
}

#[test]
fn compiled_bytecode_runs_on_the_real_vm_and_matches_the_interpreter_normal_case() {
    let Some(vm) = find_vm_binary() else {
        eprintln!(
            "skip: compilers/bytecode-vm's 'vm' binary not found next to this crate -- \
             build it first (make -C ../bytecode-vm build) to exercise this test"
        );
        return;
    };

    let src = "let a = 0; let b = 1; let i = 0; \
               while (i < 8) { print(a); let t = a + b; a = b; b = t; i = i + 1; }";
    let interpreter_output = run_source(src).unwrap();
    let asm = compile_source(src).unwrap();

    // Not run from ../bytecode-vm's own directory -- `vm` is invoked by
    // its full relative-to-here path (Rust's Command searches PATH for
    // a bare name even with current_dir set, it does NOT implicitly
    // resolve relative to current_dir), so the bytecode file path can
    // just be relative to here too.
    let bytecode_path = "../bytecode-vm/tmp_tiny_language_test.bytecode";
    std::fs::write(bytecode_path, &asm).unwrap();
    let result = Command::new(&vm).arg(bytecode_path).output();
    std::fs::remove_file(bytecode_path).ok();

    // A `vm`/`vm.exe` file existing doesn't mean it's runnable *here* --
    // e.g. a Linux ELF binary left over from a WSL build, found by
    // find_vm_binary() but not something native Windows can exec at
    // all. That's still "no compatible VM available," not a bug in the
    // compiler being tested, so it's a skip, not a panic.
    let output = match result {
        Ok(o) => o,
        Err(e) => {
            eprintln!("skip: found {vm:?} but couldn't run it here ({e}) -- likely built for a different OS");
            return;
        }
    };
    assert!(output.status.success(), "vm exited with an error: {:?}", output.status);
    let stdout = String::from_utf8(output.stdout).unwrap();
    let vm_output: Vec<i64> = stdout.lines().map(|l| l.parse().unwrap()).collect();

    assert_eq!(
        vm_output, interpreter_output,
        "compiled-and-VM-executed output diverged from the tree-walking interpreter"
    );
    println!("ok: compiled bytecode, run on the real VM, produced identical output to the interpreter: {vm_output:?}");
}
