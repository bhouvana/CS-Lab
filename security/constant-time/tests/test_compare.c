// Plain assert-based tests, no framework.
#include "compare.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_equal_buffers_normal_case(void) {
    const unsigned char a[] = "same-secret-value";
    assert(insecure_compare(a, a, sizeof(a)) == 1);
    assert(constant_time_compare(a, a, sizeof(a)) == 1);
    printf("ok: identical buffers compare equal under both functions\n");
}

static void test_differ_at_first_byte_normal_case(void) {
    const unsigned char a[] = "AAAAAAAAAA";
    const unsigned char b[] = "XAAAAAAAAA";
    assert(insecure_compare(a, b, sizeof(a)) == 0);
    assert(constant_time_compare(a, b, sizeof(a)) == 0);
    printf("ok: buffers differing at byte 0 compare unequal under both functions\n");
}

static void test_differ_at_last_byte_normal_case(void) {
    const unsigned char a[] = "AAAAAAAAAA";
    const unsigned char b[] = "AAAAAAAAAX";
    assert(insecure_compare(a, b, sizeof(a)) == 0);
    assert(constant_time_compare(a, b, sizeof(a)) == 0);
    printf("ok: buffers differing only at the last byte compare unequal under both functions\n");
}

static void test_empty_buffers_edge_case(void) {
    // Vacuously equal: there's nothing to differ on.
    assert(insecure_compare(NULL, NULL, 0) == 1);
    assert(constant_time_compare(NULL, NULL, 0) == 1);
    printf("ok: zero-length buffers compare equal (vacuously)\n");
}

static void test_single_byte_edge_case(void) {
    unsigned char a = 0x42, b = 0x42, c = 0x43;
    assert(insecure_compare(&a, &b, 1) == 1);
    assert(constant_time_compare(&a, &b, 1) == 1);
    assert(insecure_compare(&a, &c, 1) == 0);
    assert(constant_time_compare(&a, &c, 1) == 0);
    printf("ok: single-byte buffers compare correctly\n");
}

static void test_all_bytes_differ_regression(void) {
    unsigned char a[16], b[16];
    for (int i = 0; i < 16; i++) {
        a[i] = (unsigned char)i;
        b[i] = (unsigned char)(255 - i);
    }
    assert(insecure_compare(a, b, 16) == 0);
    assert(constant_time_compare(a, b, 16) == 0);
    printf("ok: buffers differing in every byte compare unequal\n");
}

static void test_both_functions_agree_on_random_like_inputs_regression(void) {
    // A small exhaustive-ish check: both functions must always agree,
    // never just "usually."
    unsigned char a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    unsigned char b[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    for (int flip = 0; flip < 8; flip++) {
        b[flip] ^= 0xFF; // corrupt one byte at a time
        int r1 = insecure_compare(a, b, 8);
        int r2 = constant_time_compare(a, b, 8);
        assert(r1 == r2);
        assert(r1 == 0);
        b[flip] ^= 0xFF; // restore
    }
    printf("ok: both functions agree across every single-byte-mismatch position\n");
}

int main(void) {
    test_equal_buffers_normal_case();
    test_differ_at_first_byte_normal_case();
    test_differ_at_last_byte_normal_case();
    test_empty_buffers_edge_case();
    test_single_byte_edge_case();
    test_all_bytes_differ_regression();
    test_both_functions_agree_on_random_like_inputs_regression();
    printf("all tests passed\n");
    return 0;
}
