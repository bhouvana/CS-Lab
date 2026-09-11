// CLI:
//   sha256 <text>                  hash the literal argument
//   sha256 --file <path>           hash a file's contents
//   sha256 --sha224 <text>         same, using SHA-224 instead
//   sha256 --sha224 --file <path>
#include "sha256.h"

#include <stdio.h>
#include <string.h>

static int hash_file(const char *path, SHA256_CTX *ctx) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    uint8_t buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) sha256_update(ctx, buf, n);
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    int use_224 = 0;
    int i = 1;
    if (argc >= 2 && strcmp(argv[1], "--sha224") == 0) {
        use_224 = 1;
        i = 2;
    }

    if (i + 1 == argc) { // one remaining plain-text argument
        if (use_224) {
            uint8_t digest[28];
            char hex[57];
            sha224_hash((const uint8_t *)argv[i], strlen(argv[i]), digest);
            sha224_to_hex(digest, hex);
            printf("%s\n", hex);
        } else {
            uint8_t digest[32];
            char hex[65];
            sha256_hash((const uint8_t *)argv[i], strlen(argv[i]), digest);
            sha256_to_hex(digest, hex);
            printf("%s\n", hex);
        }
        return 0;
    }

    if (i + 2 == argc && strcmp(argv[i], "--file") == 0) {
        SHA256_CTX ctx;
        if (use_224) sha224_init(&ctx);
        else sha256_init(&ctx);
        if (hash_file(argv[i + 1], &ctx) != 0) {
            fprintf(stderr, "cannot open %s\n", argv[i + 1]);
            return 1;
        }
        if (use_224) {
            uint8_t digest[28];
            char hex[57];
            sha224_final(&ctx, digest);
            sha224_to_hex(digest, hex);
            printf("%s\n", hex);
        } else {
            uint8_t digest[32];
            char hex[65];
            sha256_final(&ctx, digest);
            sha256_to_hex(digest, hex);
            printf("%s\n", hex);
        }
        return 0;
    }

    fprintf(stderr, "usage:\n  %s [--sha224] <text>\n  %s [--sha224] --file <path>\n", argv[0], argv[0]);
    return 2;
}
