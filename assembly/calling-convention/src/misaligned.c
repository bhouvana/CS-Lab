#include <stdio.h>

extern void misaligned_printf(double value);

int main(void) {
    puts("calling printf with a deliberately misaligned stack:");
    misaligned_printf(3.141592653589793);
    puts("libc tolerated the misalignment on this run");
    return 0;
}