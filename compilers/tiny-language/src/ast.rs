//! tokens -> AST shape (built by the parser, walked by the interpreter)

#[derive(Debug, Clone)]
pub enum Expr {
    Int(i64),
    Var(String),
    Neg(Box<Expr>),
    Binary(Box<Expr>, BinOp, Box<Expr>),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BinOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Lt,
    Le,
    Gt,
    Ge,
    Eq,
    Ne,
}

#[derive(Debug, Clone)]
pub enum Stmt {
    Let(String, Expr),
    Assign(String, Expr),
    Print(Expr),
    /// else branch is an empty Vec when there was no `else`.
    If(Expr, Vec<Stmt>, Vec<Stmt>),
    While(Expr, Vec<Stmt>),
    /// A bare `{ ... }` block, its own child scope. Currently produced
    /// only by desugaring `for` (see parser.rs) -- there's no surface
    /// syntax for a standalone block on its own.
    Block(Vec<Stmt>),
}
