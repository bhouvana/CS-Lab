// Experiment: how does allocation pattern affect fragmentation?
#include "allocator.h"

#include <stdio.h>

#define MAX_BLOCKS 4096

// Fills the arena to exhaustion with `block_size`-byte blocks (so
// there's no leftover unallocated tail masking the effect below), then
// returns how many were allocated.
static int fill_arena(void* blocks[], size_t block_size) {
    int count = 0;
    while (count < MAX_BLOCKS) {
        void* p = my_malloc(block_size);
        if (!p) break;
        blocks[count++] = p;
    }
    return count;
}

// Frees every other block (checkerboard). This is the classic
// fragmentation-inducing pattern: half the heap becomes free, but as
// scattered single-block-sized holes between still-live allocations,
// so a request even slightly bigger than one hole fails despite
// plenty of total free memory.
static void checkerboard_pattern(size_t block_size) {
    my_heap_reset();
    void* blocks[MAX_BLOCKS];
    int count = fill_arena(blocks, block_size);
    for (int i = 0; i < count; i += 2) {
        my_free(blocks[i]);
        blocks[i] = NULL;
    }

    AllocatorStats s = my_heap_stats();
    printf("Checkerboard (%d blocks x %zu bytes): free=%zu bytes across %zu blocks, "
           "largest=%zu bytes, fragmentation=%.1f%%\n",
           count, block_size, s.free_bytes, s.num_free_blocks, s.largest_free_block, 100.0 * s.fragmentation);

    void* big = my_malloc(block_size * 3); // 3 blocks' worth, contiguous
    printf("  -> request for %zu contiguous bytes (3 blocks' worth): %s\n\n", block_size * 3,
           big ? "SUCCEEDED" : "FAILED (fragmentation, even though total free bytes are enough)");
}

// Same starting point (arena filled to exhaustion), but freed in
// strict LIFO order so every free() immediately coalesces with an
// already-free neighbor -- fragmentation should stay at zero.
static void lifo_pattern(size_t block_size) {
    my_heap_reset();
    void* blocks[MAX_BLOCKS];
    int count = fill_arena(blocks, block_size);
    for (int i = count - 1; i >= 0; i--) my_free(blocks[i]);

    AllocatorStats s = my_heap_stats();
    printf("LIFO free order (%d blocks x %zu bytes): free=%zu bytes across %zu blocks, "
           "largest=%zu bytes, fragmentation=%.1f%%\n\n",
           count, block_size, s.free_bytes, s.num_free_blocks, s.largest_free_block, 100.0 * s.fragmentation);
}

int main(void) {
    printf("Benchmark: allocation pattern vs. fragmentation (arena filled to exhaustion)\n\n");
    checkerboard_pattern(1024);
    lifo_pattern(1024);
    return 0;
}
