//! AtlasLang pipeline:
//!
//!   source -> lexer -> tokens -> parser -> AST -> interpreter
//!
//! Deliberately tiny: integers, variables, arithmetic, comparisons, if,
//! while, print. No functions, classes, generics, modules, or a type
//! system — see the lab README for why.

pub mod ast;
pub mod interpreter;
pub mod lexer;
pub mod parser;

use interpreter::Interpreter;
use parser::Parser;

/// Runs source through the whole pipeline and returns every value
/// passed to print(), in order. Used by the CLI's `run` mode and by
/// the integration tests.
pub fn run_source(source: &str) -> Result<Vec<i64>, String> {
    let tokens = lexer::tokenize(source).map_err(|e| format!("lex error (line {}): {}", e.line, e.message))?;
    let mut parser = Parser::new(tokens);
    let program = parser.parse_program().map_err(|e| format!("parse error: {e}"))?;
    let mut interp = Interpreter::new();
    interp.run(&program).map_err(|e| format!("runtime error: {e}"))?;
    Ok(interp.output)
}
