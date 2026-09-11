// Plain assert-based tests, no framework.
//
// fileno()/dup()/dup2() are POSIX, not ISO C; -std=c11 alone hides them
// on glibc. _POSIX_C_SOURCE must be defined before the first system
// header pulls in its feature-test-macro guards.
#define _POSIX_C_SOURCE 200809L
#include "vm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // dup/dup2 -- available on both glibc and MinGW-w64

// Runs a program with PRINT/trace output captured to a scratch file
// instead of real stdout, so tests can assert on exact output text.
// (Not tmpfile(): on Windows it tries to create the file at the drive
// root and fails without admin rights — a local file is portable.)
static void run_and_capture(const Program *p, int trace, char *buf, size_t buf_size) {
    const char *scratch = "tests/tmp_capture.out";
    FILE *tmp = fopen(scratch, "w+");
    assert(tmp != NULL);
    VM vm;
    vm_init(&vm, p);
    vm_run(&vm, trace, tmp);
    rewind(tmp);
    size_t n = fread(buf, 1, buf_size - 1, tmp);
    buf[n] = '\0';
    fclose(tmp);
    remove(scratch);
}

static void test_push_add_print_normal_case(void) {
    Instruction code[] = {
        {OP_PUSH, 10}, {OP_PUSH, 20}, {OP_ADD, 0}, {OP_PRINT, 0}, {OP_HALT, 0},
    };
    Program p = {code, 5};
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "30\n") == 0);
    printf("ok: push/add/print -> %s", out);
}

static void test_mod_normal_case(void) {
    Instruction code[] = {
        {OP_PUSH, 17}, {OP_PUSH, 5}, {OP_MOD, 0}, {OP_PRINT, 0}, {OP_HALT, 0},
    };
    Program p = {code, 5};
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "2\n") == 0); // 17 % 5 = 2
    printf("ok: mod -> %s", out);
}

static void test_mod_by_zero_invalid_case(void) {
    Instruction code[] = {
        {OP_PUSH, 1}, {OP_PUSH, 0}, {OP_MOD, 0}, {OP_HALT, 0},
    };
    Program p = {code, 4};
    VM vm;
    vm_init(&vm, &p);
    assert(vm_run(&vm, 0, stdout) == -1);
    printf("ok: modulo by zero rejected\n");
}

static void test_comparison_opcodes_normal_case(void) {
    // Each: push a, push b, compare, print -- 6 comparisons, 2 cases
    // each (true and false), so every opcode is exercised both ways.
    struct {
        Opcode op;
        int64_t a, b;
        int64_t expected;
    } cases[] = {
        {OP_LT, 3, 5, 1}, {OP_LT, 5, 3, 0}, {OP_LE, 5, 5, 1}, {OP_LE, 6, 5, 0},
        {OP_GT, 5, 3, 1}, {OP_GT, 3, 5, 0}, {OP_GE, 5, 5, 1}, {OP_GE, 4, 5, 0},
        {OP_EQ, 7, 7, 1}, {OP_EQ, 7, 8, 0}, {OP_NE, 7, 8, 1}, {OP_NE, 7, 7, 0},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Instruction code[] = {
            {OP_PUSH, cases[i].a}, {OP_PUSH, cases[i].b}, {cases[i].op, 0}, {OP_PRINT, 0}, {OP_HALT, 0},
        };
        Program p = {code, 5};
        char out[64];
        run_and_capture(&p, 0, out, sizeof(out));
        char expected[8];
        snprintf(expected, sizeof(expected), "%lld\n", (long long)cases[i].expected);
        assert(strcmp(out, expected) == 0);
    }
    printf("ok: all 6 comparison opcodes (LT/LE/GT/GE/EQ/NE) correct both ways\n");
}

static void test_sub_mul_div_normal_case(void) {
    // (10 - 3) * 2 / 7 = 2
    Instruction code[] = {
        {OP_PUSH, 10}, {OP_PUSH, 3},  {OP_SUB, 0}, {OP_PUSH, 2},
        {OP_MUL, 0},   {OP_PUSH, 7},  {OP_DIV, 0}, {OP_PRINT, 0}, {OP_HALT, 0},
    };
    Program p = {code, 9};
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "2\n") == 0);
    printf("ok: sub/mul/div -> %s", out);
}

static void test_load_store_roundtrip(void) {
    Instruction code[] = {
        {OP_PUSH, 5}, {OP_STORE, 0}, {OP_LOAD, 0}, {OP_PRINT, 0}, {OP_HALT, 0},
    };
    Program p = {code, 5};
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "5\n") == 0);
    printf("ok: load/store roundtrip -> %s", out);
}

static void test_unconditional_jump_skips_instructions(void) {
    // PUSH 1; JUMP 3; PUSH 999 (skipped); PRINT; HALT -> prints 1
    Instruction code[] = {
        {OP_PUSH, 1}, {OP_JUMP, 3}, {OP_PUSH, 999}, {OP_PRINT, 0}, {OP_HALT, 0},
    };
    Program p = {code, 5};
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "1\n") == 0);
    printf("ok: unconditional jump -> %s", out);
}

static void test_jump_if_false_taken(void) {
    // condition 0 (false) -> jump straight to the "else" push.
    Instruction code[] = {
        {OP_PUSH, 0}, {OP_JUMP_IF_FALSE, 4}, {OP_PUSH, 111}, {OP_PRINT, 0},
        {OP_PUSH, 222}, {OP_PRINT, 0}, {OP_HALT, 0},
    };
    Program p = {code, 7};
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "222\n") == 0);
    printf("ok: jump_if_false taken -> %s", out);
}

static void test_jump_if_false_not_taken(void) {
    // condition 1 (true) -> falls through, both PUSHes execute.
    Instruction code[] = {
        {OP_PUSH, 1}, {OP_JUMP_IF_FALSE, 4}, {OP_PUSH, 111}, {OP_PRINT, 0},
        {OP_PUSH, 222}, {OP_PRINT, 0}, {OP_HALT, 0},
    };
    Program p = {code, 7};
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "111\n222\n") == 0);
    printf("ok: jump_if_false not taken -> %s", out);
}

static void test_countdown_loop_regression(void) {
    // The exact program in examples/countdown.bytecode: prints 5..1.
    Instruction code[] = {
        {OP_PUSH, 5}, {OP_STORE, 0}, {OP_LOAD, 0}, {OP_JUMP_IF_FALSE, 11},
        {OP_LOAD, 0}, {OP_PRINT, 0}, {OP_LOAD, 0}, {OP_PUSH, 1},
        {OP_SUB, 0},  {OP_STORE, 0}, {OP_JUMP, 2}, {OP_HALT, 0},
    };
    Program p = {code, 12};
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "5\n4\n3\n2\n1\n") == 0);
    printf("ok: countdown loop -> %s", out);
}

static void test_division_by_zero_invalid_case(void) {
    Instruction code[] = {{OP_PUSH, 1}, {OP_PUSH, 0}, {OP_DIV, 0}, {OP_HALT, 0}};
    Program p = {code, 4};
    VM vm;
    vm_init(&vm, &p);
    assert(vm_run(&vm, 0, stdout) == -1);
    printf("ok: division by zero rejected\n");
}

static void test_stack_underflow_invalid_case(void) {
    Instruction code[] = {{OP_ADD, 0}, {OP_HALT, 0}}; // ADD with nothing pushed
    Program p = {code, 2};
    VM vm;
    vm_init(&vm, &p);
    assert(vm_run(&vm, 0, stdout) == -1);
    printf("ok: stack underflow rejected\n");
}

static void test_missing_halt_edge_case(void) {
    Instruction code[] = {{OP_PUSH, 1}}; // falls off the end
    Program p = {code, 1};
    VM vm;
    vm_init(&vm, &p);
    assert(vm_run(&vm, 0, stdout) == -1);
    printf("ok: missing HALT rejected\n");
}

static void test_invalid_jump_target_invalid_case(void) {
    Instruction code[] = {{OP_JUMP, 99}, {OP_HALT, 0}};
    Program p = {code, 2};
    VM vm;
    vm_init(&vm, &p);
    assert(vm_run(&vm, 0, stdout) == -1);
    printf("ok: invalid jump target rejected\n");
}

static void test_assembler_normal_case(void) {
    Program p;
    assert(assemble_file("examples/add.bytecode", &p) == 0);
    assert(p.count == 5);
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "30\n") == 0);
    printf("ok: assembler loads examples/add.bytecode -> %s", out);
    free(p.code);
}

static void test_invalid_opcode_invalid_case(void) {
    // Not reachable through the text assembler (it only ever emits named
    // mnemonics) -- constructed directly, the way any other embedder of
    // this public Program/Instruction API could. Regression for the
    // switch in vm_run() having no `default`, which used to silently
    // skip an unrecognized opcode and keep running instead of rejecting it.
    Instruction code[] = {{(Opcode)99, 0}};
    Program p = {code, 1};
    VM vm;
    vm_init(&vm, &p);
    assert(vm_run(&vm, 0, stdout) == -1);
    printf("ok: invalid opcode rejected\n");
}

// Deterministic malformed-input fuzzing (CS-LAB.md §11): no libFuzzer in
// this environment, so instead a fixed-seed PRNG generates a large,
// reproducible corpus of straight-line programs -- random opcodes
// (including out-of-range ones), random operands, random length -- and
// asserts only that vm_run() always terminates cleanly, via 0 or -1,
// never a crash. Deliberately no JUMP/JUMP_IF_FALSE in the generated
// mix: a random backward jump could build a genuine infinite loop, which
// belongs in a future timeout-guarded harness, not in a test suite that
// must finish. Every other opcode's bounds-checking is exercised freely,
// including the invalid-opcode path above.
#define FUZZ_SEED 20260911u
#define FUZZ_PROGRAMS 20000

// rand_r() is POSIX-only (not guaranteed on MinGW); a small xorshift32
// keeps this test's randomness identical on every platform this repo
// builds on, which matters since "deterministic" is the whole point.
static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

static void test_random_programs_never_crash_fuzz(void) {
    static const Opcode fuzzable[] = {
        OP_PUSH, OP_POP, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_LOAD, OP_STORE, OP_PRINT, OP_HALT,
    };
    uint32_t seed = FUZZ_SEED;
    FILE *sink = fopen("tests/tmp_fuzz.out", "w");
    assert(sink != NULL);

    // Rejecting malformed input means vm_run() reports it -- to stderr,
    // always, regardless of `out` -- so 20000 mostly-malformed programs
    // would otherwise flood the console with expected, not diagnostic,
    // noise. Redirect fd 2 into the sink for the loop, then restore the
    // real stderr via a saved dup -- fd-level, so it survives even if
    // something in between calls fflush/freopen on the FILE* itself.
    fflush(stderr);
    int saved_stderr = dup(2);
    assert(saved_stderr != -1);
    dup2(fileno(sink), 2);

    for (int p = 0; p < FUZZ_PROGRAMS; p++) {
        int count = 1 + (int)(xorshift32(&seed) % 16);
        Instruction code[16];
        for (int i = 0; i < count; i++) {
            // ~10% of instructions are an intentionally out-of-range
            // opcode (100..109), not just the well-formed set.
            uint32_t roll = xorshift32(&seed) % 100;
            Opcode op = (roll < 10) ? (Opcode)(100 + roll) : fuzzable[xorshift32(&seed) % 10];
            int64_t operand = (int64_t)(xorshift32(&seed) % 512) - 256;
            code[i] = (Instruction){op, operand};
        }
        Program prog = {code, (size_t)count};
        VM vm;
        vm_init(&vm, &prog);
        int result = vm_run(&vm, 0, sink);
        assert(result == 0 || result == -1); // never anything else, never a crash
    }

    fflush(stderr);
    dup2(saved_stderr, 2);
    close(saved_stderr);

    fclose(sink);
    remove("tests/tmp_fuzz.out");
    printf("ok: %d random straight-line programs (seed %u) -> vm_run always returns 0 or -1\n", FUZZ_PROGRAMS,
           FUZZ_SEED);
}

static void write_file(const char *path, const char *contents) {
    FILE *f = fopen(path, "w");
    assert(f != NULL);
    fputs(contents, f);
    fclose(f);
}

static void test_assembler_unknown_mnemonic_invalid_case(void) {
    write_file("tests/tmp_bad_mnemonic.bytecode", "PUSH 1\nBOGUS\nHALT\n");
    Program p;
    assert(assemble_file("tests/tmp_bad_mnemonic.bytecode", &p) == -1);
    remove("tests/tmp_bad_mnemonic.bytecode");
    printf("ok: unknown mnemonic rejected\n");
}

static void test_assembler_missing_operand_invalid_case(void) {
    write_file("tests/tmp_missing_operand.bytecode", "PUSH\nHALT\n");
    Program p;
    assert(assemble_file("tests/tmp_missing_operand.bytecode", &p) == -1);
    remove("tests/tmp_missing_operand.bytecode");
    printf("ok: missing operand rejected\n");
}

static void test_assembler_recognizes_new_opcodes_normal_case(void) {
    write_file("tests/tmp_new_opcodes.bytecode", "PUSH 10\nPUSH 3\nMOD\nPUSH 2\nLT\nPRINT\nHALT\n");
    Program p;
    assert(assemble_file("tests/tmp_new_opcodes.bytecode", &p) == 0);
    char out[64];
    run_and_capture(&p, 0, out, sizeof(out));
    assert(strcmp(out, "1\n") == 0); // (10 % 3) < 2  ->  1 < 2  ->  1 (true)
    remove("tests/tmp_new_opcodes.bytecode");
    free(p.code);
    printf("ok: assembler recognizes MOD/LT/LE/GT/GE/EQ/NE mnemonics\n");
}

static void test_empty_program_edge_case(void) {
    write_file("tests/tmp_empty.bytecode", "# just a comment\n\n");
    Program p;
    assert(assemble_file("tests/tmp_empty.bytecode", &p) == 0);
    assert(p.count == 0);
    VM vm;
    vm_init(&vm, &p);
    assert(vm_run(&vm, 0, stdout) == -1); // pc=0 >= count=0, no HALT reached
    remove("tests/tmp_empty.bytecode");
    free(p.code);
    printf("ok: empty program rejected at runtime (no HALT)\n");
}

int main(void) {
    test_push_add_print_normal_case();
    test_mod_normal_case();
    test_mod_by_zero_invalid_case();
    test_comparison_opcodes_normal_case();
    test_assembler_recognizes_new_opcodes_normal_case();
    test_sub_mul_div_normal_case();
    test_load_store_roundtrip();
    test_unconditional_jump_skips_instructions();
    test_jump_if_false_taken();
    test_jump_if_false_not_taken();
    test_countdown_loop_regression();
    test_division_by_zero_invalid_case();
    test_stack_underflow_invalid_case();
    test_missing_halt_edge_case();
    test_invalid_jump_target_invalid_case();
    test_assembler_normal_case();
    test_assembler_unknown_mnemonic_invalid_case();
    test_assembler_missing_operand_invalid_case();
    test_empty_program_edge_case();
    test_invalid_opcode_invalid_case();
    test_random_programs_never_crash_fuzz();
    printf("all tests passed\n");
    return 0;
}
