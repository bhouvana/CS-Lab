// Experiment: how does allocation pattern affect fragmentation?
#include "allocator.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_BLOCKS 4096

// Fills the arena to exhaustion with `block_size`-byte blocks (so
// there's no leftover unallocated tail masking the effect below), then
// returns how many were allocated.
static int fill_arena(void* blocks[], size_t block_size, AllocatorFit fit) {
    int count = 0;
    while (count < MAX_BLOCKS) {
        void* p = my_malloc_fit(block_size, fit);
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
static void checkerboard_pattern(size_t block_size, AllocatorFit fit, const char* name) {
    my_heap_reset();
    void* blocks[MAX_BLOCKS];
    int count = fill_arena(blocks, block_size, fit);
    for (int i = 0; i < count; i += 2) {
        my_free(blocks[i]);
        blocks[i] = NULL;
    }

    AllocatorStats s = my_heap_stats();
        printf("%s checkerboard (%d blocks x %zu bytes): free=%zu bytes across %zu blocks, "
           "largest=%zu bytes, fragmentation=%.1f%%\n",
            name, count, block_size, s.free_bytes, s.num_free_blocks, s.largest_free_block, 100.0 * s.fragmentation);

    void* big = my_malloc(block_size * 3); // 3 blocks' worth, contiguous
    printf("  -> request for %zu contiguous bytes (3 blocks' worth): %s\n\n", block_size * 3,
           big ? "SUCCEEDED" : "FAILED (fragmentation, even though total free bytes are enough)");
}

// Same starting point (arena filled to exhaustion), but freed in
// strict LIFO order so every free() immediately coalesces with an
// already-free neighbor -- fragmentation should stay at zero.
static void lifo_pattern(size_t block_size, AllocatorFit fit, const char* name) {
    my_heap_reset();
    void* blocks[MAX_BLOCKS];
    int count = fill_arena(blocks, block_size, fit);
    for (int i = count - 1; i >= 0; i--) my_free(blocks[i]);

    AllocatorStats s = my_heap_stats();
    printf("%s LIFO free order (%d blocks x %zu bytes): free=%zu bytes across %zu blocks, "
           "largest=%zu bytes, fragmentation=%.1f%%\n\n",
           name, count, block_size, s.free_bytes, s.num_free_blocks, s.largest_free_block, 100.0 * s.fragmentation);
}

static void internal_fragmentation_experiment(void) {
    static const size_t requests[] = {1, 9, 17, 31, 63, 127, 255, 511, 1000};
    my_heap_reset();
    for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); i++) {
        if (!my_malloc(requests[i])) break;
    }
    AllocatorStats s = my_heap_stats();
    printf("Internal fragmentation (%zu varied requests): requested=%zu bytes, "
           "allocated capacity=%zu bytes, wasted=%zu bytes (%.1f%%)\n\n",
           sizeof(requests) / sizeof(requests[0]), s.requested_bytes, s.allocated_bytes,
           s.internal_fragmentation, 100.0 * (double)s.internal_fragmentation / s.allocated_bytes);
}

typedef struct {
    unsigned char* arena;
    size_t capacity;
    size_t used;
    size_t growths;
} GrowthModel;

static int growth_reserve(GrowthModel* model, size_t extra) {
    while (model->used + extra > model->capacity) {
        size_t new_capacity = model->capacity * 2;
        unsigned char* new_arena = realloc(model->arena, new_capacity);
        if (!new_arena) return 0;
        model->arena = new_arena;
        model->capacity = new_capacity;
        model->growths++;
    }
    model->used += extra;
    return 1;
}

static size_t growth_pattern(int lifo) {
    GrowthModel model = {malloc(64 * 1024), 64 * 1024, 0, 0};
    if (!model.arena) return 0;
    for (int i = 0; i < 1024; i++) growth_reserve(&model, 1024);
    if (lifo) {
        model.used = 0;
    } else {
        model.used /= 2;
    }
    for (int i = 0; i < 512; i++) growth_reserve(&model, 1024);
    size_t growths = model.growths;
    free(model.arena);
    return growths;
}

static void growth_experiment(void) {
    size_t checkerboard_growths = growth_pattern(0);
    size_t lifo_growths = growth_pattern(1);
    printf("Growable arena (64KB start, doubling realloc): checkerboard growths=%zu, "
           "LIFO growths=%zu\n",
           checkerboard_growths, lifo_growths);
}

int main(void) {
    printf("Benchmark: allocation pattern vs. fragmentation (arena filled to exhaustion)\n\n");
    checkerboard_pattern(1024, ALLOCATOR_FIRST_FIT, "First-fit");
    checkerboard_pattern(1024, ALLOCATOR_BEST_FIT, "Best-fit");
    lifo_pattern(1024, ALLOCATOR_FIRST_FIT, "First-fit");
    internal_fragmentation_experiment();
    growth_experiment();
    return 0;
}
