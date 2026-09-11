// Plain assert-based tests, no framework.
#include "allocator.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_allocate_and_write_normal_case(void) {
    my_heap_reset();
    int* p = my_malloc(sizeof(int) * 10);
    assert(p != NULL);
    for (int i = 0; i < 10; i++) p[i] = i * i;
    for (int i = 0; i < 10; i++) assert(p[i] == i * i);
    printf("ok: allocate and write %zu bytes\n", sizeof(int) * 10);
}

static void test_free_then_realloc_same_size_reuses_block(void) {
    my_heap_reset();
    void* a = my_malloc(64);
    AllocatorStats before = my_heap_stats();
    my_free(a);
    void* b = my_malloc(64);
    assert(a == b); // first-fit should reuse the just-freed block
    AllocatorStats after = my_heap_stats();
    assert(before.num_blocks == after.num_blocks);
    printf("ok: freeing then reallocating the same size reuses the block\n");
}

static void test_coalesce_adjacent_free_blocks(void) {
    my_heap_reset();
    void* a = my_malloc(64);
    void* b = my_malloc(64);
    void* c = my_malloc(64);
    (void)c;
    AllocatorStats before_free = my_heap_stats();
    my_free(a);
    my_free(b); // adjacent to a: should coalesce into one free block
    AllocatorStats after = my_heap_stats();
    assert(after.num_blocks < before_free.num_blocks); // two blocks merged into one
    // The merged block should be big enough for something a's or b's slot alone couldn't guarantee cleanly.
    void* merged = my_malloc(64 + 64 + 8); // roughly the combined payload (allowing for one header's worth of slack)
    assert(merged != NULL);
    printf("ok: adjacent free blocks coalesce into one\n");
}

static void test_split_leaves_remainder_available(void) {
    my_heap_reset();
    AllocatorStats initial = my_heap_stats();
    void* a = my_malloc(64); // carved from one big block; remainder becomes a new free block
    (void)a;
    AllocatorStats after = my_heap_stats();
    assert(after.num_blocks == initial.num_blocks + 1); // one alloc + one leftover free block
    printf("ok: allocating less than the whole arena splits off the remainder\n");
}

static void test_best_fit_selects_smallest_suitable_block(void) {
    my_heap_reset();
    void* first = my_malloc(64);
    void* middle = my_malloc(256);
    void* last = my_malloc(64);
    my_free(first);
    my_free(middle);
    void* best = my_malloc_best_fit(32);
    assert(best == first);
    my_free(best);
    my_free(last);
    printf("ok: best-fit selects the smallest suitable free block\n");
}

static void test_stats_report_internal_fragmentation(void) {
    my_heap_reset();
    assert(my_malloc(1) != NULL);
    assert(my_malloc(9) != NULL);
    AllocatorStats stats = my_heap_stats();
    assert(stats.requested_bytes == 10);
    assert(stats.internal_fragmentation == 14);
    printf("ok: stats report requested bytes and internal fragmentation\n");
}

static void test_calloc_zeroes_memory_normal_case(void) {
    my_heap_reset();
    unsigned char* p = my_calloc(100, 1);
    assert(p != NULL);
    for (int i = 0; i < 100; i++) assert(p[i] == 0);
    printf("ok: calloc zero-initializes memory\n");
}

static void test_realloc_grow_preserves_contents(void) {
    my_heap_reset();
    char* p = my_malloc(16);
    memcpy(p, "hello world", 12);
    char* q = my_realloc(p, 256);
    assert(q != NULL);
    assert(memcmp(q, "hello world", 12) == 0);
    printf("ok: realloc growing preserves existing contents\n");
}

static void test_realloc_null_behaves_like_malloc_edge_case(void) {
    my_heap_reset();
    void* p = my_realloc(NULL, 32);
    assert(p != NULL);
    printf("ok: realloc(NULL, n) behaves like malloc(n)\n");
}

static void test_realloc_zero_size_frees_edge_case(void) {
    my_heap_reset();
    void* p = my_malloc(32);
    AllocatorStats before = my_heap_stats();
    void* q = my_realloc(p, 0);
    assert(q == NULL);
    AllocatorStats after = my_heap_stats();
    assert(after.allocated_bytes < before.allocated_bytes);
    printf("ok: realloc(p, 0) frees p\n");
}

static void test_alignment_invalid_case(void) {
    my_heap_reset();
    void* p = my_malloc(1); // 1 byte should still come back 8-byte aligned
    assert(((size_t)p % 8) == 0);
    printf("ok: even a 1-byte allocation is 8-byte aligned\n");
}

static void test_malloc_zero_returns_null_edge_case(void) {
    my_heap_reset();
    assert(my_malloc(0) == NULL);
    printf("ok: my_malloc(0) returns NULL\n");
}

static void test_free_null_is_a_no_op_edge_case(void) {
    my_heap_reset();
    my_free(NULL); // must not crash
    printf("ok: my_free(NULL) is a safe no-op\n");
}

static void test_out_of_memory_returns_null_invalid_case(void) {
    my_heap_reset();
    void* p = my_malloc((size_t)1 << 30); // far bigger than the 1MB arena
    assert(p == NULL);
    printf("ok: an allocation larger than the arena returns NULL\n");
}

int main(void) {
    test_allocate_and_write_normal_case();
    test_free_then_realloc_same_size_reuses_block();
    test_coalesce_adjacent_free_blocks();
    test_split_leaves_remainder_available();
    test_best_fit_selects_smallest_suitable_block();
    test_stats_report_internal_fragmentation();
    test_calloc_zeroes_memory_normal_case();
    test_realloc_grow_preserves_contents();
    test_realloc_null_behaves_like_malloc_edge_case();
    test_realloc_zero_size_frees_edge_case();
    test_alignment_invalid_case();
    test_malloc_zero_returns_null_edge_case();
    test_free_null_is_a_no_op_edge_case();
    test_out_of_memory_returns_null_invalid_case();
    printf("all tests passed\n");
    return 0;
}
