//! AST -> compilers/bytecode-vm's text assembly format, as an
//! alternative to interpreter.rs's tree-walking (see the lab README's
//! Experiments section for the speed comparison this enables).
//!
//! bytecode-vm's ISA has no comparison or modulo opcodes of its own --
//! LT/LE/GT/GE/EQ/NE/MOD were added to that lab specifically to make
//! this compiler possible (see compilers/bytecode-vm's own commit
//! history and README). Everything else (PUSH/LOAD/STORE/JUMP/
//! JUMP_IF_FALSE/PRINT/HALT) already existed.
//!
//! Every distinct variable *name* gets one fixed VM memory slot,
//! allocated the first time it's seen (a `let` or, failing that, first
//! use). The VM's memory is one flat 256-slot array with no notion of
//! scope, so this compiler does NOT replicate interpreter.rs's block
//! scoping or shadowing -- two different block-scoped variables with
//! the same name would collide onto the same slot here. That's a
//! documented gap (see README), not a bug: the experiment this exists
//! for is measuring execution speed on straight-line/loop programs
//! without shadowing, not achieving full semantic parity with the
//! tree-walking interpreter.

use std::collections::HashMap;

use crate::ast::{BinOp, Expr, Stmt};

#[derive(Clone, Copy)]
enum Instr {
    Push(i64),
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
    Load(i64),
    Store(i64),
    Jump(i64),
    JumpIfFalse(i64),
    Print,
    Halt,
}

struct Compiler {
    instrs: Vec<Instr>,
    slots: HashMap<String, i64>,
    next_slot: i64,
}

/// Compiles `program` to bytecode-vm's text assembly format. Returns an
/// error (rather than panicking) on assigning to or reading a variable
/// that was never declared with `let` -- the same class of error
/// interpreter.rs catches at run time, caught here at compile time
/// instead, since the VM has no way to report "undefined variable" on
/// its own (an unallocated slot is just memory containing 0).
pub fn compile(program: &[Stmt]) -> Result<String, String> {
    let mut c = Compiler {
        instrs: Vec::new(),
        slots: HashMap::new(),
        next_slot: 0,
    };
    c.compile_block(program)?;
    c.instrs.push(Instr::Halt);
    Ok(c.render())
}

impl Compiler {
    fn slot_for_declaration(&mut self, name: &str) -> i64 {
        if let Some(&s) = self.slots.get(name) {
            return s;
        }
        let s = self.next_slot;
        self.next_slot += 1;
        self.slots.insert(name.to_string(), s);
        s
    }

    fn slot_for_use(&self, name: &str) -> Result<i64, String> {
        self.slots
            .get(name)
            .copied()
            .ok_or_else(|| format!("compile error: undefined variable '{name}'"))
    }

    fn compile_block(&mut self, stmts: &[Stmt]) -> Result<(), String> {
        for s in stmts {
            self.compile_stmt(s)?;
        }
        Ok(())
    }

    fn compile_stmt(&mut self, stmt: &Stmt) -> Result<(), String> {
        match stmt {
            Stmt::Let(name, expr) => {
                self.compile_expr(expr)?;
                let slot = self.slot_for_declaration(name);
                self.instrs.push(Instr::Store(slot));
            }
            Stmt::Assign(name, expr) => {
                self.compile_expr(expr)?;
                let slot = self.slot_for_use(name)?;
                self.instrs.push(Instr::Store(slot));
            }
            Stmt::Print(expr) => {
                self.compile_expr(expr)?;
                self.instrs.push(Instr::Print);
            }
            Stmt::If(cond, then_branch, else_branch) => {
                self.compile_expr(cond)?;
                let jf_idx = self.instrs.len();
                self.instrs.push(Instr::JumpIfFalse(0)); // patched below
                self.compile_block(then_branch)?;
                if else_branch.is_empty() {
                    let end = self.instrs.len() as i64;
                    self.instrs[jf_idx] = Instr::JumpIfFalse(end);
                } else {
                    let jmp_idx = self.instrs.len();
                    self.instrs.push(Instr::Jump(0)); // patched below
                    let else_start = self.instrs.len() as i64;
                    self.instrs[jf_idx] = Instr::JumpIfFalse(else_start);
                    self.compile_block(else_branch)?;
                    let end = self.instrs.len() as i64;
                    self.instrs[jmp_idx] = Instr::Jump(end);
                }
            }
            Stmt::While(cond, body) => {
                let loop_start = self.instrs.len() as i64;
                self.compile_expr(cond)?;
                let jf_idx = self.instrs.len();
                self.instrs.push(Instr::JumpIfFalse(0)); // patched below
                self.compile_block(body)?;
                self.instrs.push(Instr::Jump(loop_start));
                let loop_end = self.instrs.len() as i64;
                self.instrs[jf_idx] = Instr::JumpIfFalse(loop_end);
            }
            Stmt::Block(stmts) => self.compile_block(stmts)?,
        }
        Ok(())
    }

    fn compile_expr(&mut self, expr: &Expr) -> Result<(), String> {
        match expr {
            Expr::Int(v) => self.instrs.push(Instr::Push(*v)),
            Expr::Var(name) => {
                let slot = self.slot_for_use(name)?;
                self.instrs.push(Instr::Load(slot));
            }
            Expr::Neg(inner) => {
                // No dedicated negate opcode on the VM: 0 - inner.
                self.instrs.push(Instr::Push(0));
                self.compile_expr(inner)?;
                self.instrs.push(Instr::Sub);
            }
            Expr::Binary(left, op, right) => {
                self.compile_expr(left)?;
                self.compile_expr(right)?;
                self.instrs.push(match op {
                    BinOp::Add => Instr::Add,
                    BinOp::Sub => Instr::Sub,
                    BinOp::Mul => Instr::Mul,
                    BinOp::Div => Instr::Div,
                    BinOp::Mod => Instr::Mod,
                    BinOp::Lt => Instr::Lt,
                    BinOp::Le => Instr::Le,
                    BinOp::Gt => Instr::Gt,
                    BinOp::Ge => Instr::Ge,
                    BinOp::Eq => Instr::Eq,
                    BinOp::Ne => Instr::Ne,
                });
            }
        }
        Ok(())
    }

    fn render(&self) -> String {
        let mut out = String::new();
        for instr in &self.instrs {
            let line = match instr {
                Instr::Push(v) => format!("PUSH {v}"),
                Instr::Add => "ADD".to_string(),
                Instr::Sub => "SUB".to_string(),
                Instr::Mul => "MUL".to_string(),
                Instr::Div => "DIV".to_string(),
                Instr::Mod => "MOD".to_string(),
                Instr::Lt => "LT".to_string(),
                Instr::Le => "LE".to_string(),
                Instr::Gt => "GT".to_string(),
                Instr::Ge => "GE".to_string(),
                Instr::Eq => "EQ".to_string(),
                Instr::Ne => "NE".to_string(),
                Instr::Load(s) => format!("LOAD {s}"),
                Instr::Store(s) => format!("STORE {s}"),
                Instr::Jump(t) => format!("JUMP {t}"),
                Instr::JumpIfFalse(t) => format!("JUMP_IF_FALSE {t}"),
                Instr::Print => "PRINT".to_string(),
                Instr::Halt => "HALT".to_string(),
            };
            out.push_str(&line);
            out.push('\n');
        }
        out
    }
}
