#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>
#include <stdint.h>

// File-based API. Returns 0 on success, -1 on error (bad path, bad
// magic on decompress, etc). No messages are printed here — callers
// decide what to report.
int huffman_compress_file(const char *input_path, const char *output_path);
int huffman_decompress_file(const char *input_path, const char *output_path);

// In-memory API — what the file-based functions call internally, and
// what the tests exercise directly. Caller owns *out and must free() it.
// Returns 0 on success, -1 on error (e.g. corrupt/truncated header).
int huffman_compress_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len);
int huffman_decompress_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len);

#endif
