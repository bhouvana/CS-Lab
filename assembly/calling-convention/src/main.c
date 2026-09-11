// C source -> compiler -> assembly -> registers -> return value.
// The functions called here are hand-written in src/math.S, not
// compiled from C -- see that file for the register-by-register
// accounting.
#include <stdio.h>

extern long add2(long a, long b);
extern long add3(long a, long b, long c);
extern long uses_callee_saved(long a);
extern long check_rbx_preserved(void);
extern long sum_three_locals(long a, long b, long c);
extern double add_fp2(double a, double b);

int main(void) {
    printf("add2(3, 4)              = %ld\n", add2(3, 4));
    printf("add3(1, 2, 3)            = %ld\n", add3(1, 2, 3));
    printf("uses_callee_saved(5)     = %ld\n", uses_callee_saved(5));
    printf("check_rbx_preserved()    = %ld (1 = RBX survived the call, as the ABI requires)\n",
           check_rbx_preserved());
    printf("sum_three_locals(1,2,3)  = %ld\n", sum_three_locals(1, 2, 3));
    printf("add_fp2(1.5, 2.25)      = %.2f\n", add_fp2(1.5, 2.25));
    return 0;
}
