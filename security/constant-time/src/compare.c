// two byte buffers -> comparison -> equal/not-equal, but on two very
// differently-timed paths
#include "compare.h"

int insecure_compare(const unsigned char* a, const unsigned char* b, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (a[i] != b[i]) return 0; // secret-dependent branch: exits as soon as a mismatch is seen
    }
    return 1;
}

int constant_time_compare(const unsigned char* a, const unsigned char* b, size_t len) {
    unsigned char diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= (unsigned char)(a[i] ^ b[i]); // no branch: every byte contributes, win or lose
    }
    return diff == 0;
}
