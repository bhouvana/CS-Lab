#ifdef __MINGW32__
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* A small accounting model for a generational collector. */
static void experiment_generational(void) {
    enum { old_count = 9000, young_count = 1000, live_young = 100 };
    unsigned char old[old_count];
    unsigned char live[young_count];
    size_t live_old = 0;

    for (size_t i = 0; i < old_count; i++) {
        old[i] = 1;
        live_old += old[i];
    }
    for (size_t i = 0; i < young_count; i++) live[i] = (i < live_young);

    const size_t remembered_old = 1;
    size_t full_mark_work = live_old;
    for (size_t i = 0; i < young_count; i++) full_mark_work += live[i];
    size_t full_work = old_count + young_count + full_mark_work;
    size_t young_mark_work = remembered_old;
    for (size_t i = 0; i < young_count; i++) young_mark_work += live[i];
    size_t young_work = young_count + young_mark_work;

    assert(young_work < full_work);
    printf("Generational collection\n");
    printf("  full heap work:  %zu objects\n", full_work);
    printf("  young-only work: %zu objects\n", young_work);
    printf("  work reduction:  %.1f%%\n\n", 100.0 * (full_work - young_work) / full_work);
}

typedef struct RefNode {
    const char* name;
    size_t references;
    struct RefNode* child;
    size_t* freed_count;
} RefNode;

static RefNode* ref_new(const char* name, size_t* freed_count) {
    RefNode* node = calloc(1, sizeof(*node));
    assert(node != NULL);
    node->name = name;
    node->references = 1; /* the caller owns the initial reference */
    node->freed_count = freed_count;
    return node;
}

static void ref_retain(RefNode* node) {
    node->references++;
}

static void ref_release(RefNode* node) {
    if (!node || --node->references != 0) return;
    RefNode* child = node->child;
    (*node->freed_count)++;
    free(node);
    ref_release(child);
}

static void experiment_reference_counting(void) {
    size_t freed_count = 0;
    RefNode* a = ref_new("A", &freed_count);
    RefNode* b = ref_new("B", &freed_count);
    a->child = b;
    ref_retain(b);
    b->child = a;
    ref_retain(a);

    ref_release(a); /* release both outside roots */
    ref_release(b);
    assert(freed_count == 0);
    printf("Reference counting\n");
    printf("  A <-> B after roots released: %zu objects freed (cycle leaked)\n", freed_count);

    a->child = NULL; /* a real cycle collector would find and break this */
    b->child = NULL;
    ref_release(a);
    ref_release(b);
    assert(freed_count == 2);
    printf("  after breaking the cycle:     %zu objects freed\n\n", freed_count);
}

typedef struct {
    int id;
    int child_slot;
    int live;
} ArenaObject;

static size_t largest_free_run(const ArenaObject* arena, size_t slots) {
    size_t largest = 0;
    size_t current = 0;
    for (size_t i = 0; i < slots; i++) {
        if (arena[i].live) {
            current = 0;
        } else {
            current++;
            if (current > largest) largest = current;
        }
    }
    return largest;
}

static void experiment_compaction(void) {
    enum { slots = 64 };
    ArenaObject arena[slots];
    int relocation[slots];
    size_t live_count = 0;

    for (size_t i = 0; i < slots; i++) {
        arena[i].id = (int)i;
        arena[i].child_slot = (i % 2 == 0 && i + 2 < slots) ? (int)(i + 2) : -1;
        arena[i].live = (i % 2 == 0);
        if (arena[i].live) live_count++;
        relocation[i] = -1;
    }

    size_t free_before = slots - live_count;
    size_t largest_before = largest_free_run(arena, slots);
    double fragmentation_before = 100.0 * (free_before - largest_before) / free_before;

    size_t next = 0;
    for (size_t old = 0; old < slots; old++) {
        if (arena[old].live) {
            relocation[old] = (int)next;
            if (next != old) arena[next] = arena[old];
            next++;
        }
    }
    for (size_t i = 0; i < live_count; i++) {
        if (arena[i].child_slot >= 0) {
            arena[i].child_slot = relocation[arena[i].child_slot];
        }
    }
    for (size_t i = live_count; i < slots; i++) arena[i].live = 0;

    size_t largest_after = largest_free_run(arena, slots);
    double fragmentation_after = 100.0 * (free_before - largest_after) / free_before;
    assert(largest_before == 1);
    assert(largest_after == free_before);
    assert(arena[0].child_slot == 1);
    printf("Compacting sweep\n");
    printf("  before: %zu free slots, largest=%zu, fragmentation=%.1f%%\n", free_before,
           largest_before, fragmentation_before);
    printf("  after:  %zu free slots, largest=%zu, fragmentation=%.1f%%\n", free_before,
           largest_after, fragmentation_after);
}

int main(void) {
    experiment_generational();
    experiment_reference_counting();
    experiment_compaction();
    return 0;
}