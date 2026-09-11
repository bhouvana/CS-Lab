// Small CLI demo: allocate a handful of blocks, free some, show stats.
#include "allocator.h"

#include <stdio.h>

static void print_stats(const char* label) {
    AllocatorStats s = my_heap_stats();
    printf("%s\n", label);
    printf("  allocated bytes: %zu\n", s.allocated_bytes);
    printf("  free bytes:      %zu\n", s.free_bytes);
    printf("  blocks:          %zu (%zu free)\n", s.num_blocks, s.num_free_blocks);
    printf("  largest free:    %zu\n", s.largest_free_block);
    printf("  fragmentation:   %.2f%%\n\n", 100.0 * s.fragmentation);
}

int main(void) {
    print_stats("Fresh heap");

    void* a = my_malloc(1000);
    void* b = my_malloc(2000);
    void* c = my_malloc(3000);
    (void)a;
    (void)c;
    print_stats("After 3 allocations (1000, 2000, 3000 bytes)");

    my_free(b);
    print_stats("After freeing the middle block (2000 bytes)");

    void* d = my_malloc(1500);
    (void)d;
    print_stats("After allocating 1500 bytes (should reuse the freed block's space)");

    return 0;
}
