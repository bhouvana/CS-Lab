#ifndef VM_H
#define VM_H

// MinGW's printf/scanf default to a non-ISO mode that doesn't recognize
// %zu/%lld; this switches it to the ISO-C99-compatible implementation.
// A no-op on Linux/glibc, where this was never an issue.
#ifdef __MINGW32__
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    OP_PUSH,
    OP_POP,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    // Comparisons: pop b then a, push 1 if "a OP b" holds, else 0 --
    // added so a real language (see compilers/tiny-language's
    // compile-to-bytecode experiment) can compile `if`/`while`
    // conditions at all; nothing upstream of this needs booleans as
    // anything but the existing 0/1 integers already on the stack.
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_EQ,
    OP_NE,
    OP_LOAD,
    OP_STORE,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    // Subroutines: CALL pushes the return address (the instruction
    // right after it) onto a separate call stack and jumps to its
    // target; RET pops that address and jumps back. A *separate*
    // stack from the data one -- so a callee's own PUSH/POP traffic
    // can never corrupt a pending return address, and vice versa.
    // Arguments/return values still go through ordinary memory slots
    // (STORE before CALL, LOAD after) -- there's no calling convention
    // beyond that.
    OP_CALL,
    OP_RET,
    OP_PRINT,
    OP_HALT,
} Opcode;

typedef struct {
    Opcode op;
    // Meaning depends on op: PUSH = value, LOAD/STORE = memory slot,
    // JUMP/JUMP_IF_FALSE = target instruction index. Unused otherwise.
    int64_t operand;
} Instruction;

typedef struct {
    Instruction *code;
    size_t count;
} Program;

// e.g. OP_PUSH -> "PUSH". Used by the assembler's error messages and by
// --trace output.
const char *opcode_name(Opcode op);

// Parses a text bytecode program: one instruction per line, "#" starts
// a line comment, blank lines ignored, jump targets are raw 0-based
// instruction indices (no labels — see README). Returns 0 on success
// (caller must free(program->code)); on error prints a message with
// line number to stderr and returns -1.
int assemble_file(const char *path, Program *program);

#define STACK_MAX 1024
#define MEMORY_SLOTS 256
#define CALL_STACK_MAX 256

typedef struct {
    int64_t stack[STACK_MAX];
    int sp; // number of values currently on the stack
    int64_t memory[MEMORY_SLOTS];
    size_t call_stack[CALL_STACK_MAX]; // return addresses, pushed by CALL, popped by RET
    int call_sp;
    const Program *program;
    size_t pc;
} VM;

void vm_init(VM *vm, const Program *program);

// Runs fetch/decode/execute until HALT or an error. If trace is
// non-zero, each instruction and the resulting stack are written to
// `out` (so PRINT output and trace output can be redirected
// independently of the process's real stdout — see bench.c). Returns 0
// on HALT, -1 on runtime error (stack overflow/underflow, division by
// zero, bad jump target/memory slot, or running off the end without a
// HALT).
int vm_run(VM *vm, int trace, FILE *out);

#endif
