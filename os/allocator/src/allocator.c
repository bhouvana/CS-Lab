// malloc() -> heap (a fixed arena, carved into blocks) -> free list
#include "allocator.h"

#include <string.h>

#define ARENA_SIZE (1 << 20) // 1 MB fixed arena; no sbrk/mmap growth (see README)
#define ALIGNMENT 8
#define MIN_SPLIT_PAYLOAD (2 * sizeof(void*)) // below this, don't bother splitting off a sliver

typedef struct Block {
    size_t size; // payload capacity in bytes (aligned), NOT including this header
    int is_free;
    struct Block *addr_prev, *addr_next; // whole-heap neighbors, in address order (for coalescing)
    struct Block *free_prev, *free_next; // free list links (meaningful only while is_free)
} Block;

static unsigned char arena[ARENA_SIZE];
static Block* heap_head = NULL; // first block in address order
static Block* free_head = NULL; // head of the free list (unordered)

static size_t align_up(size_t n) {
    return (n + (ALIGNMENT - 1)) & ~(size_t)(ALIGNMENT - 1);
}

static void heap_init(void) {
    heap_head = (Block*)arena;
    heap_head->size = ARENA_SIZE - sizeof(Block);
    heap_head->is_free = 1;
    heap_head->addr_prev = NULL;
    heap_head->addr_next = NULL;
    heap_head->free_prev = NULL;
    heap_head->free_next = NULL;
    free_head = heap_head;
}

void my_heap_reset(void) {
    heap_init();
}

static void free_list_remove(Block* b) {
    if (b->free_prev) b->free_prev->free_next = b->free_next;
    else free_head = b->free_next;
    if (b->free_next) b->free_next->free_prev = b->free_prev;
    b->free_prev = b->free_next = NULL;
}

static void free_list_push(Block* b) {
    b->free_prev = NULL;
    b->free_next = free_head;
    if (free_head) free_head->free_prev = b;
    free_head = b;
}

// Splits `b` (which must be free and unlinked from the free list by
// the caller before this returns it to the free list) into a
// `wanted`-byte block and, if enough is left over, a second free
// block holding the remainder.
static void split_block(Block* b, size_t wanted) {
    size_t remainder = b->size - wanted;
    if (remainder < sizeof(Block) + MIN_SPLIT_PAYLOAD) return; // not worth splitting

    Block* new_block = (Block*)((unsigned char*)(b + 1) + wanted);
    new_block->size = remainder - sizeof(Block);
    new_block->is_free = 1;
    new_block->addr_prev = b;
    new_block->addr_next = b->addr_next;
    if (new_block->addr_next) new_block->addr_next->addr_prev = new_block;
    b->addr_next = new_block;
    b->size = wanted;

    free_list_push(new_block);
}

void* my_malloc(size_t size) {
    if (size == 0) return NULL;
    if (!heap_head) heap_init();

    size_t wanted = align_up(size);

    for (Block* b = free_head; b != NULL; b = b->free_next) {
        if (b->size >= wanted) {
            free_list_remove(b);
            split_block(b, wanted); // shrinks b->size to `wanted` if it splits
            b->is_free = 0;
            return (void*)(b + 1);
        }
    }
    return NULL; // arena exhausted / too fragmented to satisfy this request
}

static void coalesce(Block* b) {
    if (b->addr_next && b->addr_next->is_free) {
        Block* n = b->addr_next;
        free_list_remove(n);
        b->size += sizeof(Block) + n->size;
        b->addr_next = n->addr_next;
        if (b->addr_next) b->addr_next->addr_prev = b;
    }
    if (b->addr_prev && b->addr_prev->is_free) {
        Block* p = b->addr_prev;
        free_list_remove(p);
        p->size += sizeof(Block) + b->size;
        p->addr_next = b->addr_next;
        if (p->addr_next) p->addr_next->addr_prev = p;
        b = p;
    }
    free_list_push(b);
}

void my_free(void* ptr) {
    if (!ptr) return;
    Block* b = (Block*)ptr - 1;
    b->is_free = 1;
    coalesce(b);
}

void* my_calloc(size_t nmemb, size_t size) {
    if (nmemb != 0 && size > (size_t)-1 / nmemb) return NULL; // overflow guard
    size_t total = nmemb * size;
    void* p = my_malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void* my_realloc(void* ptr, size_t size) {
    if (!ptr) return my_malloc(size);
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    Block* b = (Block*)ptr - 1;
    size_t wanted = align_up(size);

    if (wanted <= b->size) return ptr; // already big enough; keep it simple, don't shrink-split

    // Try growing in place by absorbing an immediately-following free block.
    if (b->addr_next && b->addr_next->is_free && b->size + sizeof(Block) + b->addr_next->size >= wanted) {
        Block* n = b->addr_next;
        free_list_remove(n);
        b->size += sizeof(Block) + n->size;
        b->addr_next = n->addr_next;
        if (b->addr_next) b->addr_next->addr_prev = b;
        split_block(b, wanted);
        return ptr;
    }

    // Fall back: allocate elsewhere, copy, free the old block.
    void* new_ptr = my_malloc(size);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, ptr, b->size < size ? b->size : size);
    my_free(ptr);
    return new_ptr;
}

AllocatorStats my_heap_stats(void) {
    AllocatorStats s = {0, 0, 0, 0, 0, 0.0};
    if (!heap_head) heap_init();
    for (Block* b = heap_head; b != NULL; b = b->addr_next) {
        s.num_blocks++;
        if (b->is_free) {
            s.num_free_blocks++;
            s.free_bytes += b->size;
            if (b->size > s.largest_free_block) s.largest_free_block = b->size;
        } else {
            s.allocated_bytes += b->size;
        }
    }
    s.fragmentation = s.free_bytes > 0 ? 1.0 - (double)s.largest_free_block / (double)s.free_bytes : 0.0;
    return s;
}
