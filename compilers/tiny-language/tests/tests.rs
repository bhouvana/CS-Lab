use tiny_language::run_source;

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
