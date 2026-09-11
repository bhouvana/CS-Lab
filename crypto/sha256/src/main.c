// CLI:
//   sha256 <text>          hash the literal argument
//   sha256 --file <path>   hash a file's contents
#include "sha256.h"

#include <stdio.h>
#include <string.h>

static int hash_file(const char *path, uint8_t digest[32]) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    SHA256_CTX ctx;
    sha256_init(&ctx);
    uint8_t buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) sha256_update(&ctx, buf, n);
    fclose(f);
    sha256_final(&ctx, digest);
    return 0;
}

int main(int argc, char **argv) {
    uint8_t digest[32];
    char hex[65];

    if (argc == 2) {
        sha256_hash((const uint8_t *)argv[1], strlen(argv[1]), digest);
        sha256_to_hex(digest, hex);
        printf("%s\n", hex);
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "--file") == 0) {
        if (hash_file(argv[2], digest) != 0) {
            fprintf(stderr, "cannot open %s\n", argv[2]);
            return 1;
        }
        sha256_to_hex(digest, hex);
        printf("%s\n", hex);
        return 0;
    }

    fprintf(stderr, "usage:\n  %s <text>\n  %s --file <path>\n", argv[0], argv[0]);
    return 2;
}
