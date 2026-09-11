// CLI:
//   huffman compress   <in> <out>
//   huffman decompress <in> <out>
#include <stdio.h>
#include <string.h>

#include "huffman.h"

static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

static int usage(const char *prog) {
    fprintf(stderr, "usage:\n  %s compress   <in> <out>\n  %s decompress <in> <out>\n", prog, prog);
    return 2;
}

int main(int argc, char **argv) {
    if (argc != 4) return usage(argv[0]);
    const char *mode = argv[1];
    const char *in_path = argv[2];
    const char *out_path = argv[3];

    if (strcmp(mode, "compress") == 0) {
        if (huffman_compress_file(in_path, out_path) != 0) {
            fprintf(stderr, "compress failed: could not read %s\n", in_path);
            return 1;
        }
        long orig = file_size(in_path);
        long comp = file_size(out_path);
        printf("Original:   %ld bytes\n", orig);
        printf("Compressed: %ld bytes\n", comp);
        if (orig > 0) printf("Ratio:      %.2f%%\n", 100.0 * (double)comp / (double)orig);
        return 0;
    }

    if (strcmp(mode, "decompress") == 0) {
        if (huffman_decompress_file(in_path, out_path) != 0) {
            fprintf(stderr, "decompress failed: %s is not a valid .huf file\n", in_path);
            return 1;
        }
        return 0;
    }

    return usage(argv[0]);
}
