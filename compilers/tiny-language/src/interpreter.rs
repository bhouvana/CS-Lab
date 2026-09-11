//! AST -> execution (tree-walking interpreter). Block-scoped: every
//! if/while/for body is its own child scope on a scope stack, not one
//! flat global namespace -- `let` declares into the innermost scope,
//! `assign` and variable reads search outward until they find it.

use std::collections::HashMap;

use crate::ast::{BinOp, Expr, Stmt};

pub struct Interpreter {
    /// scopes[0] is the top-level (global) scope, never popped. Each
    /// if/while/for body execution pushes one child scope and pops it
    /// on the way out -- so a `let` inside a loop body redeclares (and
    /// resets) fresh every iteration, exactly like a real block-scoped
    /// language, while an outer variable mutated via `assign` from
    /// inside the block keeps its value across iterations.
    scopes: Vec<HashMap<String, i64>>,
    /// Every value passed to print(), in order — lets tests assert on
    /// program output without scraping stdout.
    pub output: Vec<i64>,
}

impl Default for Interpreter {
    fn default() -> Self {
        Self::new()
    }
}

impl Interpreter {
    pub fn new() -> Self {
        Interpreter {
            scopes: vec![HashMap::new()],
            output: Vec::new(),
        }
    }

    pub fn run(&mut self, program: &[Stmt]) -> Result<(), String> {
        for stmt in program {
            self.exec(stmt)?;
        }
        Ok(())
    }

    /// Runs `block` in a fresh child scope, popped again whether it
    /// exits normally or via an Err -- the one place scope depth
    /// actually changes.
    fn run_block(&mut self, block: &[Stmt]) -> Result<(), String> {
        self.scopes.push(HashMap::new());
        let result = self.run(block);
        self.scopes.pop();
        result
    }

    fn exec(&mut self, stmt: &Stmt) -> Result<(), String> {
        match stmt {
            Stmt::Let(name, expr) => {
                let value = self.eval(expr)?;
                self.scopes
                    .last_mut()
                    .expect("global scope always present")
                    .insert(name.clone(), value);
            }
            Stmt::Assign(name, expr) => {
                let value = self.eval(expr)?;
                for scope in self.scopes.iter_mut().rev() {
                    if let std::collections::hash_map::Entry::Occupied(mut e) = scope.entry(name.clone()) {
                        e.insert(value);
                        return Ok(());
                    }
                }
                return Err(format!("assignment to undeclared variable '{name}' (use 'let' first)"));
            }
            Stmt::Print(expr) => {
                let value = self.eval(expr)?;
                println!("{value}");
                self.output.push(value);
            }
            Stmt::If(cond, then_branch, else_branch) => {
                if self.eval(cond)? != 0 {
                    self.run_block(then_branch)?;
                } else {
                    self.run_block(else_branch)?;
                }
            }
            Stmt::While(cond, body) => {
                while self.eval(cond)? != 0 {
                    self.run_block(body)?;
                }
            }
            Stmt::Block(stmts) => self.run_block(stmts)?,
        }
        Ok(())
    }

    fn eval(&self, expr: &Expr) -> Result<i64, String> {
        match expr {
            Expr::Int(v) => Ok(*v),
            Expr::Var(name) => self
                .scopes
                .iter()
                .rev()
                .find_map(|s| s.get(name).copied())
                .ok_or_else(|| format!("undefined variable '{name}'")),
            Expr::Neg(inner) => Ok(-self.eval(inner)?),
            Expr::Binary(left, op, right) => {
                let l = self.eval(left)?;
                let r = self.eval(right)?;
                Ok(match op {
                    BinOp::Add => l + r,
                    BinOp::Sub => l - r,
                    BinOp::Mul => l * r,
                    BinOp::Div => {
                        if r == 0 {
                            return Err("division by zero".to_string());
                        }
                        l / r
                    }
                    BinOp::Mod => {
                        if r == 0 {
                            return Err("modulo by zero".to_string());
                        }
                        l % r
                    }
                    BinOp::Lt => (l < r) as i64,
                    BinOp::Le => (l <= r) as i64,
                    BinOp::Gt => (l > r) as i64,
                    BinOp::Ge => (l >= r) as i64,
                    BinOp::Eq => (l == r) as i64,
                    BinOp::Ne => (l != r) as i64,
                })
            }
        }
    }
}
