// objects + roots -> mark phase -> sweep phase -> reachable vs collected
#include "gc.h"

#include <stdlib.h>
#include <string.h>

void gc_init(GC* gc) {
    memset(gc, 0, sizeof(*gc));
}

Object* gc_new_object(GC* gc, const char* name) {
    Object* obj = calloc(1, sizeof(Object));
    strncpy(obj->name, name, sizeof(obj->name) - 1);

    if (gc->heap_count == gc->heap_cap) {
        gc->heap_cap = (gc->heap_cap == 0) ? 8 : gc->heap_cap * 2;
        gc->heap = realloc(gc->heap, gc->heap_cap * sizeof(Object*));
    }
    gc->heap[gc->heap_count++] = obj;
    return obj;
}

void gc_add_root(GC* gc, Object* obj) {
    if (gc->root_count == gc->root_cap) {
        gc->root_cap = (gc->root_cap == 0) ? 8 : gc->root_cap * 2;
        gc->roots = realloc(gc->roots, gc->root_cap * sizeof(Object*));
    }
    gc->roots[gc->root_count++] = obj;
}

void gc_remove_root(GC* gc, Object* obj) {
    for (size_t i = 0; i < gc->root_count; i++) {
        if (gc->roots[i] == obj) {
            gc->roots[i] = gc->roots[gc->root_count - 1];
            gc->root_count--;
            return;
        }
    }
}

void gc_add_reference(Object* from, Object* to) {
    if (from->num_children == from->children_cap) {
        from->children_cap = (from->children_cap == 0) ? 4 : from->children_cap * 2;
        from->children = realloc(from->children, from->children_cap * sizeof(Object*));
    }
    from->children[from->num_children++] = to;
}

// Iterative DFS via an explicit worklist, not recursion: a naive
// recursive mark() would recurse as deep as the longest reference
// chain, and a long live linked list (exactly the shape
// src/bench.c stresses) would overflow the call stack. The
// already-marked check makes this cycle-safe either way.
static void mark(Object* root) {
    if (!root || root->marked) return;

    size_t cap = 64, count = 0;
    Object** worklist = malloc(cap * sizeof(Object*));
    worklist[count++] = root;
    root->marked = 1;

    while (count > 0) {
        Object* obj = worklist[--count];
        for (size_t i = 0; i < obj->num_children; i++) {
            Object* child = obj->children[i];
            if (child && !child->marked) {
                child->marked = 1;
                if (count == cap) {
                    cap *= 2;
                    worklist = realloc(worklist, cap * sizeof(Object*));
                }
                worklist[count++] = child;
            }
        }
    }
    free(worklist);
}

CollectStats gc_collect(GC* gc) {
    CollectStats stats = {0, 0, 0};
    stats.objects_before = gc->heap_count;

    for (size_t i = 0; i < gc->root_count; i++) mark(gc->roots[i]);

    size_t write = 0;
    for (size_t read = 0; read < gc->heap_count; read++) {
        Object* obj = gc->heap[read];
        if (obj->marked) {
            obj->marked = 0; // reset for the next collection cycle
            gc->heap[write++] = obj;
        } else {
            free(obj->children);
            free(obj);
            stats.objects_collected++;
        }
    }
    gc->heap_count = write;
    stats.objects_retained = gc->heap_count;
    return stats;
}

int gc_is_live(const GC* gc, const char* name) {
    for (size_t i = 0; i < gc->heap_count; i++)
        if (strcmp(gc->heap[i]->name, name) == 0) return 1;
    return 0;
}

void gc_destroy(GC* gc) {
    for (size_t i = 0; i < gc->heap_count; i++) {
        free(gc->heap[i]->children);
        free(gc->heap[i]);
    }
    free(gc->heap);
    free(gc->roots);
    memset(gc, 0, sizeof(*gc));
}
