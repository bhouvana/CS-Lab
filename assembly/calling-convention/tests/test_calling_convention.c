// Plain assert-based tests, no framework.
#include <assert.h>
#include <math.h>
#include <stdio.h>

extern long add2(long a, long b);
extern long add3(long a, long b, long c);
extern long uses_callee_saved(long a);
extern long check_rbx_preserved(void);
extern long sum_three_locals(long a, long b, long c);
extern double add_fp2(double a, double b);

static void test_add2_normal_case(void) {
    assert(add2(3, 4) == 7);
    assert(add2(-5, 5) == 0);
    printf("ok: add2 (2-argument register passing)\n");
}

static void test_add3_three_arguments(void) {
    assert(add3(1, 2, 3) == 6);
    printf("ok: add3 (3-argument register passing: rdi, rsi, rdx)\n");
}

static void test_uses_callee_saved_normal_case(void) {
    assert(uses_callee_saved(5) == 10);
    assert(uses_callee_saved(0) == 0);
    printf("ok: uses_callee_saved computes correctly despite touching RBX\n");
}

static void test_callee_saved_contract_is_actually_honored(void) {
    // This is the real point of the lab: not just "the function
    // computes the right answer" but "RBX genuinely survives the call,
    // as the ABI requires" -- verified at runtime, not asserted in a
    // comment.
    assert(check_rbx_preserved() == 1);
    printf("ok: RBX (callee-saved) is provably preserved across a call\n");
}

static void test_locals_and_alignment_edge_case(void) {
    // Negative numbers exercise the same stack-slot spill/reload path
    // as positive ones, with no special-casing possible in the asm.
    assert(sum_three_locals(1, 2, 3) == 6);
    assert(sum_three_locals(-10, 5, 5) == 0);
    assert(sum_three_locals(0, 0, 0) == 0);
    printf("ok: sum_three_locals (locals + aligned nested CALL)\n");
}

static void test_floating_point_register_passing(void) {
    assert(fabs(add_fp2(1.5, 2.25) - 3.75) < 1e-12);
    assert(fabs(add_fp2(-2.5, 0.5) + 2.0) < 1e-12);
    printf("ok: add_fp2 (XMM0/XMM1 floating-point register passing)\n");
}

int main(void) {
    test_add2_normal_case();
    test_add3_three_arguments();
    test_uses_callee_saved_normal_case();
    test_callee_saved_contract_is_actually_honored();
    test_locals_and_alignment_edge_case();
    test_floating_point_register_passing();
    printf("all tests passed\n");
    return 0;
}
