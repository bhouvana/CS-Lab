#ifndef GC_H
#define GC_H

// MinGW's printf defaults to a non-ISO mode that doesn't recognize
// %zu; this switches it to the ISO-C99-compatible implementation. A
// no-op on Linux/glibc, where this was never an issue.
#ifdef __MINGW32__
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include <stddef.h>

// Educational mark-and-sweep garbage collector. Objects form a graph
// (via gc_add_reference); a "root set" defines what's directly
// reachable from outside the graph (local variables, globals, etc. in
// a real runtime). gc_collect() marks everything reachable from the
// roots, then frees everything that wasn't marked.

typedef struct Object {
    char name[32];
    int marked;
    struct Object** children;
    size_t num_children;
    size_t children_cap;
} Object;

typedef struct {
    Object** heap; // every currently-live object this GC owns
    size_t heap_count;
    size_t heap_cap;

    Object** roots; // objects directly reachable from "outside"
    size_t root_count;
    size_t root_cap;
} GC;

void gc_init(GC* gc);
void gc_destroy(GC* gc); // frees every remaining object; call once, at the end

// Allocates a new object, registers it in the heap. Not reachable from
// any root by default -- it's garbage until gc_add_root() or
// gc_add_reference() makes it reachable.
Object* gc_new_object(GC* gc, const char* name);

void gc_add_root(GC* gc, Object* obj);
// Removes obj from the root set (simulates it going out of scope). A
// no-op if obj isn't currently a root.
void gc_remove_root(GC* gc, Object* obj);

// Adds a directed edge `from -> to` (from references to).
void gc_add_reference(Object* from, Object* to);

typedef struct {
    size_t objects_before;
    size_t objects_retained;
    size_t objects_collected;
} CollectStats;

// Mark phase (DFS from every root, cycle-safe via the marked flag)
// then sweep phase (free everything left unmarked). Returns before
// collection stats are known.
CollectStats gc_collect(GC* gc);

// True if an object with this name is currently live in the heap.
// Looks up by name rather than by pointer so callers never need to
// dereference (or even compare) a pointer that gc_collect() may have
// already freed.
int gc_is_live(const GC* gc, const char* name);

#endif
