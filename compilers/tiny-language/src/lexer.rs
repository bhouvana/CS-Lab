//! source -> tokens

#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    Int(i64),
    Ident(String),
    Let,
    Print,
    If,
    Else,
    While,
    For,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Assign,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    LParen,
    RParen,
    LBrace,
    RBrace,
    Semi,
    Eof,
}

#[derive(Debug)]
pub struct LexError {
    pub message: String,
    pub line: usize,
}

/// Tokenizes the whole source up front (no streaming) — simple, and the
/// language is small enough that this is never a bottleneck. Each token
/// carries the source line it started on, for error messages.
pub fn tokenize(source: &str) -> Result<Vec<(Token, usize)>, LexError> {
    let chars: Vec<char> = source.chars().collect();
    let mut i = 0;
    let mut line = 1;
    let mut tokens = Vec::new();

    while i < chars.len() {
        let c = chars[i];
        match c {
            ' ' | '\t' | '\r' => i += 1,
            '\n' => {
                line += 1;
                i += 1;
            }
            '#' => {
                while i < chars.len() && chars[i] != '\n' {
                    i += 1;
                }
            }
            '0'..='9' => {
                let start = i;
                while i < chars.len() && chars[i].is_ascii_digit() {
                    i += 1;
                }
                let text: String = chars[start..i].iter().collect();
                let value: i64 = text.parse().map_err(|_| LexError {
                    message: format!("invalid integer literal '{text}'"),
                    line,
                })?;
                tokens.push((Token::Int(value), line));
            }
            'a'..='z' | 'A'..='Z' | '_' => {
                let start = i;
                while i < chars.len() && (chars[i].is_ascii_alphanumeric() || chars[i] == '_') {
                    i += 1;
                }
                let text: String = chars[start..i].iter().collect();
                let tok = match text.as_str() {
                    "let" => Token::Let,
                    "print" => Token::Print,
                    "if" => Token::If,
                    "else" => Token::Else,
                    "while" => Token::While,
                    "for" => Token::For,
                    _ => Token::Ident(text),
                };
                tokens.push((tok, line));
            }
            '+' => {
                tokens.push((Token::Plus, line));
                i += 1;
            }
            '-' => {
                tokens.push((Token::Minus, line));
                i += 1;
            }
            '*' => {
                tokens.push((Token::Star, line));
                i += 1;
            }
            '/' => {
                tokens.push((Token::Slash, line));
                i += 1;
            }
            '%' => {
                tokens.push((Token::Percent, line));
                i += 1;
            }
            '(' => {
                tokens.push((Token::LParen, line));
                i += 1;
            }
            ')' => {
                tokens.push((Token::RParen, line));
                i += 1;
            }
            '{' => {
                tokens.push((Token::LBrace, line));
                i += 1;
            }
            '}' => {
                tokens.push((Token::RBrace, line));
                i += 1;
            }
            ';' => {
                tokens.push((Token::Semi, line));
                i += 1;
            }
            '=' => {
                if chars.get(i + 1) == Some(&'=') {
                    tokens.push((Token::Eq, line));
                    i += 2;
                } else {
                    tokens.push((Token::Assign, line));
                    i += 1;
                }
            }
            '!' => {
                if chars.get(i + 1) == Some(&'=') {
                    tokens.push((Token::Ne, line));
                    i += 2;
                } else {
                    return Err(LexError {
                        message: "unexpected '!' (did you mean '!='?)".into(),
                        line,
                    });
                }
            }
            '<' => {
                if chars.get(i + 1) == Some(&'=') {
                    tokens.push((Token::Le, line));
                    i += 2;
                } else {
                    tokens.push((Token::Lt, line));
                    i += 1;
                }
            }
            '>' => {
                if chars.get(i + 1) == Some(&'=') {
                    tokens.push((Token::Ge, line));
                    i += 2;
                } else {
                    tokens.push((Token::Gt, line));
                    i += 1;
                }
            }
            other => {
                return Err(LexError {
                    message: format!("unexpected character '{other}'"),
                    line,
                })
            }
        }
    }
    tokens.push((Token::Eof, line));
    Ok(tokens)
}
