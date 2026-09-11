// Plain assert-based tests, no framework.
#include "gc.h"

#include <assert.h>
#include <stdio.h>

static void test_directive_example_normal_case(void) {
    GC gc;
    gc_init(&gc);
    Object* a = gc_new_object(&gc, "A");
    Object* b = gc_new_object(&gc, "B");
    Object* c = gc_new_object(&gc, "C");
    gc_new_object(&gc, "D");

    gc_add_reference(a, b);
    gc_add_root(&gc, a);
    gc_add_root(&gc, c);

    CollectStats s = gc_collect(&gc);
    assert(s.objects_before == 4);
    assert(s.objects_collected == 1); // only D
    assert(s.objects_retained == 3);
    assert(gc_is_live(&gc, "A") && gc_is_live(&gc, "B") && gc_is_live(&gc, "C"));
    assert(!gc_is_live(&gc, "D"));
    gc_destroy(&gc);
    printf("ok: directive's example -> A/B/C retained, D collected\n");
}

static void test_dropping_root_cascades_to_children(void) {
    GC gc;
    gc_init(&gc);
    Object* a = gc_new_object(&gc, "A");
    Object* b = gc_new_object(&gc, "B");
    gc_add_reference(a, b);
    gc_add_root(&gc, a);
    gc_collect(&gc); // both retained

    gc_remove_root(&gc, a);
    CollectStats s = gc_collect(&gc);
    assert(s.objects_collected == 2); // A and B both unreachable now
    assert(!gc_is_live(&gc, "A") && !gc_is_live(&gc, "B"));
    gc_destroy(&gc);
    printf("ok: dropping a root cascades to everything only it reached\n");
}

static void test_cycle_does_not_infinite_loop_edge_case(void) {
    // A <-> B, a cycle with no root -- must not hang, and both must be
    // collected (mark-and-sweep's whole point vs. naive refcounting).
    GC gc;
    gc_init(&gc);
    Object* a = gc_new_object(&gc, "A");
    Object* b = gc_new_object(&gc, "B");
    gc_add_reference(a, b);
    gc_add_reference(b, a);
    // no roots at all

    CollectStats s = gc_collect(&gc);
    assert(s.objects_collected == 2);
    gc_destroy(&gc);
    printf("ok: an unreachable reference cycle is collected without hanging\n");
}

static void test_rooted_cycle_is_retained(void) {
    GC gc;
    gc_init(&gc);
    Object* a = gc_new_object(&gc, "A");
    Object* b = gc_new_object(&gc, "B");
    gc_add_reference(a, b);
    gc_add_reference(b, a);
    gc_add_root(&gc, a);

    CollectStats s = gc_collect(&gc);
    assert(s.objects_collected == 0);
    assert(gc_is_live(&gc, "A") && gc_is_live(&gc, "B"));
    gc_destroy(&gc);
    printf("ok: a reachable reference cycle is retained, not collected\n");
}

static void test_long_chain_does_not_overflow_the_stack_regression(void) {
    // A naive recursive mark() recurses as deep as the longest
    // reference chain; this chain is deep enough to have crashed an
    // earlier, recursive version of mark() on a typical 1-8MB thread
    // stack. Iterative marking (an explicit worklist) must handle it.
    GC gc;
    gc_init(&gc);
    const int depth = 200000;
    Object* prev = NULL;
    for (int i = 0; i < depth; i++) {
        Object* o = gc_new_object(&gc, "link");
        if (prev) gc_add_reference(prev, o);
        else gc_add_root(&gc, o);
        prev = o;
    }
    CollectStats s = gc_collect(&gc);
    assert(s.objects_collected == 0);
    assert(s.objects_retained == (size_t)depth);
    gc_destroy(&gc);
    printf("ok: a %d-deep reference chain marks without a stack overflow\n", depth);
}

static void test_empty_heap_edge_case(void) {
    GC gc;
    gc_init(&gc);
    CollectStats s = gc_collect(&gc);
    assert(s.objects_before == 0 && s.objects_collected == 0 && s.objects_retained == 0);
    gc_destroy(&gc);
    printf("ok: collecting an empty heap is a safe no-op\n");
}

static void test_double_collect_is_idempotent_invalid_case(void) {
    // Collecting twice in a row with nothing changed in between should
    // find nothing new to collect the second time.
    GC gc;
    gc_init(&gc);
    Object* a = gc_new_object(&gc, "A");
    gc_add_root(&gc, a);
    gc_collect(&gc);
    CollectStats s2 = gc_collect(&gc);
    assert(s2.objects_collected == 0 && s2.objects_retained == 1);
    gc_destroy(&gc);
    printf("ok: collecting twice with no changes collects nothing new\n");
}

int main(void) {
    test_directive_example_normal_case();
    test_dropping_root_cascades_to_children();
    test_cycle_does_not_infinite_loop_edge_case();
    test_rooted_cycle_is_retained();
    test_long_chain_does_not_overflow_the_stack_regression();
    test_empty_heap_edge_case();
    test_double_collect_is_idempotent_invalid_case();
    printf("all tests passed\n");
    return 0;
}
