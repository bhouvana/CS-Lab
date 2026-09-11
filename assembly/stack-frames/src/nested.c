// main() -> foo() -> bar(), each with a local variable, compiled with
// frame pointers kept (-fno-omit-frame-pointer) so RBP forms a real,
// walkable linked list of stack frames. `noinline` forces genuine
// CALL instructions instead of the compiler flattening everything.
#include <stdio.h>

__attribute__((noinline)) int bar(int x) {
    int local_bar = x * 2;
    printf("bar: local_bar = %d\n", local_bar);
    return local_bar; // <- breakpoint target: all three frames are live here
}

__attribute__((noinline)) int foo(int x) {
    int local_foo = x + 10;
    printf("foo: local_foo = %d\n", local_foo);
    return bar(local_foo);
}

int main(void) {
    int local_main = 5;
    printf("main: local_main = %d\n", local_main);
    int result = foo(local_main);
    printf("result = %d\n", result);
    return 0;
}
