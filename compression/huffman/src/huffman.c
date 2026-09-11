// Huffman compression pipeline:
//
//   input -> frequency table -> Huffman tree -> canonical codes
//         -> bit packing -> compressed file
//
// The compressed format stores only the 256 code *lengths* (one byte
// each, 0 = symbol unused), not the tree and not the codes themselves.
// Canonical Huffman codes are a pure, deterministic function of the
// lengths (see assign_canonical_codes), so the decoder rebuilds the
// exact same codes the encoder used from that 256-byte table alone.
//
// Header layout (268 bytes, all multi-byte fields little-endian):
//   4 bytes   magic "HUF1"
//   8 bytes   original size in bytes
//   256 bytes code length per symbol (0 if unused)
// followed by the packed bitstream (MSB-first within each byte, padded
// with zero bits to the next byte boundary).

#include "huffman.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC "HUF1"
#define HEADER_SIZE (4 + 8 + 256)
#define MAX_SYMBOLS 256

// ---------------------------------------------------------------------
// Huffman tree
// ---------------------------------------------------------------------

// Up to 256 leaves + up to 255 internal nodes.
typedef struct {
    uint64_t freq;
    int left, right; // -1 = no child
    int symbol;      // 0..255 for leaves, -1 for internal nodes
} Node;

// Builds the tree by repeatedly combining the two lowest-frequency
// active nodes. O(n^2) over at most 256 symbols (~65k comparisons worst
// case) — a binary heap would be faster, but at this alphabet size the
// simple scan is just as fast in practice and much easier to read.
// Returns the root index, or -1 if there were no symbols at all.
static int build_tree(const uint64_t freq[MAX_SYMBOLS], Node nodes[2 * MAX_SYMBOLS - 1]) {
    int active[2 * MAX_SYMBOLS - 1];
    int active_count = 0;
    int n = 0;

    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (freq[i] > 0) {
            nodes[n].freq = freq[i];
            nodes[n].left = -1;
            nodes[n].right = -1;
            nodes[n].symbol = i;
            active[active_count++] = n;
            n++;
        }
    }

    if (active_count == 0) return -1;

    if (active_count == 1) {
        // A single distinct symbol still needs a real (1-bit) code, so
        // give it a trivial internal parent.
        nodes[n].freq = nodes[active[0]].freq;
        nodes[n].left = active[0];
        nodes[n].right = -1;
        nodes[n].symbol = -1;
        return n;
    }

    while (active_count > 1) {
        int a = 0, b = 1;
        if (nodes[active[b]].freq < nodes[active[a]].freq) {
            int t = a;
            a = b;
            b = t;
        }
        for (int i = 2; i < active_count; i++) {
            uint64_t f = nodes[active[i]].freq;
            if (f < nodes[active[a]].freq) {
                b = a;
                a = i;
            } else if (f < nodes[active[b]].freq) {
                b = i;
            }
        }

        int ia = active[a], ib = active[b];
        nodes[n].freq = nodes[ia].freq + nodes[ib].freq;
        nodes[n].left = ia;
        nodes[n].right = ib;
        nodes[n].symbol = -1;

        int hi = a > b ? a : b, lo = a > b ? b : a;
        active[hi] = active[--active_count];
        active[lo] = active[--active_count];
        active[active_count++] = n;
        n++;
    }

    return active[0];
}

static void compute_lengths(const Node *nodes, int idx, int depth, uint8_t lengths[MAX_SYMBOLS]) {
    if (nodes[idx].left == -1 && nodes[idx].right == -1) {
        lengths[nodes[idx].symbol] = (uint8_t)depth;
        return;
    }
    if (nodes[idx].left != -1) compute_lengths(nodes, nodes[idx].left, depth + 1, lengths);
    if (nodes[idx].right != -1) compute_lengths(nodes, nodes[idx].right, depth + 1, lengths);
}

// ---------------------------------------------------------------------
// Canonical codes — deterministic given only the lengths, so both the
// encoder and decoder call this and always agree.
// ---------------------------------------------------------------------

static void assign_canonical_codes(const uint8_t lengths[MAX_SYMBOLS], uint32_t codes[MAX_SYMBOLS]) {
    int length_count[MAX_SYMBOLS] = {0};
    for (int s = 0; s < MAX_SYMBOLS; s++)
        if (lengths[s] > 0) length_count[lengths[s]]++;

    uint32_t next_code[MAX_SYMBOLS] = {0};
    uint32_t code = 0;
    for (int len = 1; len < MAX_SYMBOLS; len++) {
        code = (code + (uint32_t)length_count[len - 1]) << 1;
        next_code[len] = code;
    }

    for (int s = 0; s < MAX_SYMBOLS; s++)
        if (lengths[s] > 0) codes[s] = next_code[lengths[s]]++;
}

// ---------------------------------------------------------------------
// Bit I/O
// ---------------------------------------------------------------------

typedef struct {
    uint8_t *buf;
    size_t len;
    size_t cap;
    uint8_t cur;    // partially filled byte
    int cur_bits;   // bits used in `cur`, 0..7
} BitWriter;

static void bw_init(BitWriter *bw) {
    bw->cap = 4096;
    bw->buf = malloc(bw->cap);
    bw->len = 0;
    bw->cur = 0;
    bw->cur_bits = 0;
}

static void bw_push_byte(BitWriter *bw, uint8_t byte) {
    if (bw->len == bw->cap) {
        bw->cap *= 2;
        bw->buf = realloc(bw->buf, bw->cap);
    }
    bw->buf[bw->len++] = byte;
}

static void bw_put_bit(BitWriter *bw, int bit) {
    bw->cur = (uint8_t)((bw->cur << 1) | (bit & 1));
    bw->cur_bits++;
    if (bw->cur_bits == 8) {
        bw_push_byte(bw, bw->cur);
        bw->cur = 0;
        bw->cur_bits = 0;
    }
}

static void bw_put_code(BitWriter *bw, uint32_t code, int length) {
    for (int i = length - 1; i >= 0; i--) bw_put_bit(bw, (int)((code >> i) & 1));
}

// Pads the final partial byte with zero bits and flushes it.
static void bw_finish(BitWriter *bw) {
    if (bw->cur_bits > 0) {
        bw->cur = (uint8_t)(bw->cur << (8 - bw->cur_bits));
        bw_push_byte(bw, bw->cur);
        bw->cur_bits = 0;
    }
}

typedef struct {
    const uint8_t *buf;
    size_t len;
    size_t byte_pos;
    int bit_pos; // next bit to read within buf[byte_pos], 0 = MSB
} BitReader;

static int br_get_bit(BitReader *br) {
    if (br->byte_pos >= br->len) return -1; // out of data
    int bit = (br->buf[br->byte_pos] >> (7 - br->bit_pos)) & 1;
    br->bit_pos++;
    if (br->bit_pos == 8) {
        br->bit_pos = 0;
        br->byte_pos++;
    }
    return bit;
}

// ---------------------------------------------------------------------
// Decode trie — built from (code, length) pairs so decoding is a
// bit-at-a-time walk rather than a per-bit scan over all 256 symbols.
// ---------------------------------------------------------------------

typedef struct {
    int child[2];
    int symbol; // -1 = internal
} TrieNode;

static int trie_new_node(TrieNode *trie, int *count) {
    trie[*count].child[0] = -1;
    trie[*count].child[1] = -1;
    trie[*count].symbol = -1;
    return (*count)++;
}

static void trie_insert(TrieNode *trie, int *count, int symbol, uint32_t code, int length) {
    int node = 0;
    for (int i = length - 1; i >= 0; i--) {
        int bit = (int)((code >> i) & 1);
        if (trie[node].child[bit] == -1) trie[node].child[bit] = trie_new_node(trie, count);
        node = trie[node].child[bit];
    }
    trie[node].symbol = symbol;
}

// ---------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------

static void write_u64_le(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

static uint64_t read_u64_le(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

int huffman_compress_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len) {
    uint64_t freq[MAX_SYMBOLS] = {0};
    for (size_t i = 0; i < in_len; i++) freq[in[i]]++;

    uint8_t lengths[MAX_SYMBOLS] = {0};
    if (in_len > 0) {
        Node nodes[2 * MAX_SYMBOLS - 1];
        int root = build_tree(freq, nodes);
        compute_lengths(nodes, root, 0, lengths);
    }

    uint32_t codes[MAX_SYMBOLS] = {0};
    assign_canonical_codes(lengths, codes);

    BitWriter bw;
    bw_init(&bw);
    for (size_t i = 0; i < in_len; i++) bw_put_code(&bw, codes[in[i]], lengths[in[i]]);
    bw_finish(&bw);

    uint8_t *result = malloc(HEADER_SIZE + bw.len);
    memcpy(result, MAGIC, 4);
    write_u64_le(result + 4, (uint64_t)in_len);
    memcpy(result + 12, lengths, MAX_SYMBOLS);
    memcpy(result + HEADER_SIZE, bw.buf, bw.len);

    *out = result;
    *out_len = HEADER_SIZE + bw.len;
    free(bw.buf);
    return 0;
}

int huffman_decompress_buffer(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len) {
    if (in_len < HEADER_SIZE || memcmp(in, MAGIC, 4) != 0) return -1;

    uint64_t original_size = read_u64_le(in + 4);
    const uint8_t *lengths = in + 12;

    uint8_t *result = malloc(original_size > 0 ? original_size : 1);
    if (original_size == 0) {
        *out = result;
        *out_len = 0;
        return 0;
    }

    uint32_t codes[MAX_SYMBOLS] = {0};
    assign_canonical_codes(lengths, codes);

    TrieNode trie[2 * MAX_SYMBOLS]; // generous upper bound
    int trie_count = 0;
    trie_new_node(trie, &trie_count); // root
    for (int s = 0; s < MAX_SYMBOLS; s++)
        if (lengths[s] > 0) trie_insert(trie, &trie_count, s, codes[s], lengths[s]);

    BitReader br = {in + HEADER_SIZE, in_len - HEADER_SIZE, 0, 0};
    size_t produced = 0;
    int node = 0;
    // Single-symbol input: the trie root has exactly one child and that
    // child is already the leaf, so the loop below decodes it correctly
    // without special-casing.
    while (produced < original_size) {
        int bit = br_get_bit(&br);
        if (bit < 0) { // truncated stream — should not happen for valid files
            free(result);
            return -1;
        }
        node = trie[node].child[bit];
        if (node == -1) {
            free(result);
            return -1;
        }
        if (trie[node].symbol != -1) {
            result[produced++] = (uint8_t)trie[node].symbol;
            node = 0;
        }
    }

    *out = result;
    *out_len = original_size;
    return 0;
}

static int read_whole_file(const char *path, uint8_t **buf, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return -1; }
    fseek(f, 0, SEEK_SET);
    *buf = malloc((size_t)size > 0 ? (size_t)size : 1);
    if (size > 0 && fread(*buf, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(*buf);
        return -1;
    }
    fclose(f);
    *len = (size_t)size;
    return 0;
}

static int write_whole_file(const char *path, const uint8_t *buf, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    if (len > 0 && fwrite(buf, 1, len, f) != len) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int huffman_compress_file(const char *input_path, const char *output_path) {
    uint8_t *in;
    size_t in_len;
    if (read_whole_file(input_path, &in, &in_len) != 0) return -1;

    uint8_t *out;
    size_t out_len;
    huffman_compress_buffer(in, in_len, &out, &out_len);
    free(in);

    int rc = write_whole_file(output_path, out, out_len);
    free(out);
    return rc;
}

int huffman_decompress_file(const char *input_path, const char *output_path) {
    uint8_t *in;
    size_t in_len;
    if (read_whole_file(input_path, &in, &in_len) != 0) return -1;

    uint8_t *out;
    size_t out_len;
    int rc = huffman_decompress_buffer(in, in_len, &out, &out_len);
    free(in);
    if (rc != 0) return -1;

    rc = write_whole_file(output_path, out, out_len);
    free(out);
    return rc;
}
