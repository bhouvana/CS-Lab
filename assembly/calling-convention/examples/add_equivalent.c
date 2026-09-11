// The C equivalent of src/math.S's add2 -- compile this with
// `gcc -O0 -S` and compare the generated assembly against add2 by
// hand (see the Makefile's `objdump` target and the README).
long add2_c(long a, long b) {
    return a + b;
}
