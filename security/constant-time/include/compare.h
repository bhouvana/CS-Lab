#ifndef COMPARE_H
#define COMPARE_H

// MinGW's printf defaults to a non-ISO mode that doesn't recognize
// %zu; this switches it to the ISO-C99-compatible implementation. A
// no-op on Linux/glibc, where this was never an issue.
#ifdef __MINGW32__
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include <stddef.h>

// Returns 1 if equal, 0 if not. Exits the comparison loop the moment
// a mismatch is found -- fast on average, but how fast depends on
// WHERE the first mismatch is, which is exactly the side channel this
// lab demonstrates.
int insecure_compare(const unsigned char* a, const unsigned char* b, size_t len);

// Returns 1 if equal, 0 if not. Always examines every byte and
// combines differences with bitwise OR instead of branching on them,
// so its running time does not depend on where (or whether) a and b
// differ.
int constant_time_compare(const unsigned char* a, const unsigned char* b, size_t len);

#endif
