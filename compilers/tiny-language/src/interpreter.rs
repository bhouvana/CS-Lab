//! AST -> execution (tree-walking interpreter, one flat global scope —
//! see README limitations: this language has no block scoping).

use std::collections::HashMap;

use crate::ast::{BinOp, Expr, Stmt};

pub struct Interpreter {
    vars: HashMap<String, i64>,
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
        Interpreter { vars: HashMap::new(), output: Vec::new() }
    }

    pub fn run(&mut self, program: &[Stmt]) -> Result<(), String> {
        for stmt in program {
            self.exec(stmt)?;
        }
        Ok(())
    }

    fn exec(&mut self, stmt: &Stmt) -> Result<(), String> {
        match stmt {
            Stmt::Let(name, expr) => {
                let value = self.eval(expr)?;
                self.vars.insert(name.clone(), value);
            }
            Stmt::Assign(name, expr) => {
                if !self.vars.contains_key(name) {
                    return Err(format!("assignment to undeclared variable '{name}' (use 'let' first)"));
                }
                let value = self.eval(expr)?;
                self.vars.insert(name.clone(), value);
            }
            Stmt::Print(expr) => {
                let value = self.eval(expr)?;
                println!("{value}");
                self.output.push(value);
            }
            Stmt::If(cond, then_branch, else_branch) => {
                if self.eval(cond)? != 0 {
                    self.run(then_branch)?;
                } else {
                    self.run(else_branch)?;
                }
            }
            Stmt::While(cond, body) => {
                while self.eval(cond)? != 0 {
                    self.run(body)?;
                }
            }
        }
        Ok(())
    }

    fn eval(&self, expr: &Expr) -> Result<i64, String> {
        match expr {
            Expr::Int(v) => Ok(*v),
            Expr::Var(name) => {
                self.vars.get(name).copied().ok_or_else(|| format!("undefined variable '{name}'"))
            }
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
