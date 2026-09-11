#ifndef ALLOCATOR_H
#define ALLOCATOR_H

// MinGW's printf defaults to a non-ISO mode that doesn't recognize
// %zu; this switches it to the ISO-C99-compatible implementation. A
// no-op on Linux/glibc, where this was never an issue.
#ifdef __MINGW32__
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include <stddef.h>

// Educational heap allocator over a fixed-size static arena (no
// sbrk/mmap — see README). First-fit search over an explicit free
// list, with block splitting and immediate-neighbor coalescing.

void* my_malloc(size_t size);
typedef enum {
    ALLOCATOR_FIRST_FIT,
    ALLOCATOR_BEST_FIT
} AllocatorFit;
void* my_malloc_fit(size_t size, AllocatorFit fit);
void* my_malloc_best_fit(size_t size);
void my_free(void* ptr);
void* my_calloc(size_t nmemb, size_t size);
void* my_realloc(void* ptr, size_t size);

// Resets the arena to one large free block. Mainly for tests, so each
// test starts from a clean heap instead of accumulating state.
void my_heap_reset(void);

typedef struct {
    size_t allocated_bytes;    // sum of payload sizes currently allocated
    size_t requested_bytes;     // bytes requested by callers for live allocations
    size_t internal_fragmentation; // capacity minus requested bytes in live blocks
    size_t free_bytes;         // sum of payload sizes currently free
    size_t num_blocks;         // total blocks, free + allocated
    size_t num_free_blocks;
    size_t largest_free_block; // payload size of the biggest free block
    // 0 = no fragmentation (all free memory is one block), approaching
    // 1 = the free memory is split into many blocks none of which can
    // satisfy a request anywhere near free_bytes in size.
    double fragmentation;
} AllocatorStats;

AllocatorStats my_heap_stats(void);

#endif
