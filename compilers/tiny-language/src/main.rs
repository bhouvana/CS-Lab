// CLI:
//   atlas run     <file.atlas>   execute the program
//   atlas tokens  <file.atlas>   print the token stream
//   atlas ast     <file.atlas>   print the parsed AST
//   atlas compile <file.atlas>   emit bytecode-vm assembly instead of running it
//   atlas                        pipeline demo (no args — see default_demo)
use std::env;
use std::fs;

use tiny_language::{interpreter::Interpreter, lexer, parser::Parser};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() == 1 {
        // The root Makefile's Rust convention: `cargo run --release` with
        // no arguments runs the lab's "experiment". This language's
        // interesting property is the pipeline itself being inspectable,
        // so the demo runs one example through every stage.
        default_demo();
        return;
    }
    if args.len() != 3 {
        eprintln!(
            "usage:\n  atlas run     <file.atlas>\n  atlas tokens  <file.atlas>\n  atlas ast     <file.atlas>\n  atlas compile <file.atlas>"
        );
        std::process::exit(2);
    }
    let mode = args[1].as_str();
    let path = &args[2];

    let source = match fs::read_to_string(path) {
        Ok(s) => s,
        Err(e) => {
            eprintln!("cannot read {path}: {e}");
            std::process::exit(1);
        }
    };

    let tokens = match lexer::tokenize(&source) {
        Ok(t) => t,
        Err(e) => {
            eprintln!("lex error (line {}): {}", e.line, e.message);
            std::process::exit(1);
        }
    };

    if mode == "tokens" {
        for (tok, line) in &tokens {
            println!("{line:>4}: {tok:?}");
        }
        return;
    }

    let mut parser = Parser::new(tokens);
    let program = match parser.parse_program() {
        Ok(p) => p,
        Err(e) => {
            eprintln!("parse error: {e}");
            std::process::exit(1);
        }
    };

    match mode {
        "ast" => {
            for stmt in &program {
                println!("{stmt:#?}");
            }
        }
        "run" => {
            let mut interp = Interpreter::new();
            if let Err(e) = interp.run(&program) {
                eprintln!("runtime error: {e}");
                std::process::exit(1);
            }
        }
        "compile" => match tiny_language::compiler::compile(&program) {
            Ok(asm) => print!("{asm}"),
            Err(e) => {
                eprintln!("{e}");
                std::process::exit(1);
            }
        },
        other => {
            eprintln!("unknown mode '{other}', expected run|tokens|ast|compile");
            std::process::exit(2);
        }
    }
}

/// Demonstrates every pipeline stage on one bundled example, so the
/// interesting property of this lab (each stage is directly
/// inspectable) is visible with zero setup.
fn default_demo() {
    let path = "examples/fib.atlas";
    println!("AtlasLang pipeline demo (no arguments given) -- running {path}\n");

    let source = fs::read_to_string(path).expect("bundled example should exist");
    println!("--- source ---");
    print!("{source}");

    let tokens = lexer::tokenize(&source).expect("bundled example should lex cleanly");
    println!("\n--- tokens (first 8 of {}) ---", tokens.len());
    for (tok, line) in tokens.iter().take(8) {
        println!("{line:>4}: {tok:?}");
    }

    let mut parser = Parser::new(tokens);
    let program = parser.parse_program().expect("bundled example should parse cleanly");
    println!("\n--- ast (first statement) ---");
    println!("{:#?}", program[0]);

    println!("\n--- run ---");
    let mut interp = Interpreter::new();
    interp.run(&program).expect("bundled example should run cleanly");
}
