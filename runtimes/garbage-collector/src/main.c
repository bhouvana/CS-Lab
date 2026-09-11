// Builds the project directive's exact example graph:
//
//   root
//    +-- object A
//    |    +-- object B
//    |
//    +-- object C
//
//   object D   (unrooted, unreferenced)
//
// then collects twice: once as-is, then again after dropping the root
// reference to A (which should take B down with it).
#include "gc.h"

#include <stdio.h>

static void report(const GC* gc, const char* name) {
    printf("%s -> %s\n", name, gc_is_live(gc, name) ? "retained" : "collected");
}

int main(void) {
    GC gc;
    gc_init(&gc);

    Object* a = gc_new_object(&gc, "A");
    Object* b = gc_new_object(&gc, "B");
    Object* c = gc_new_object(&gc, "C");
    gc_new_object(&gc, "D"); // never rooted, never referenced: garbage from the start

    gc_add_reference(a, b); // A -> B
    gc_add_root(&gc, a);
    gc_add_root(&gc, c);

    printf("Heap before first collection: %zu objects\n\n", gc.heap_count);

    CollectStats s1 = gc_collect(&gc);
    report(&gc, "A");
    report(&gc, "B");
    report(&gc, "C");
    report(&gc, "D");
    printf("\ncollected: %zu, retained: %zu (of %zu)\n", s1.objects_collected, s1.objects_retained,
           s1.objects_before);

    printf("\n--- dropping the root reference to A ---\n");
    printf("(B is only reachable through A, so it should become garbage too)\n\n");
    gc_remove_root(&gc, a);

    CollectStats s2 = gc_collect(&gc);
    report(&gc, "A");
    report(&gc, "B");
    report(&gc, "C");
    printf("\ncollected: %zu, retained: %zu (of %zu)\n", s2.objects_collected, s2.objects_retained,
           s2.objects_before);

    gc_destroy(&gc);
    return 0;
}
