# Huffman Compression

## What is this?

A from-scratch Huffman compressor/decompressor in C, with real binary
output (not printed codes) and support for arbitrary byte content.

```bash
./huffman compress   <in>  <out>
./huffman decompress <out> <restored>
```

## Why does it matter?

Huffman coding is the simplest real example of entropy coding: assign
short bit patterns to frequent symbols and long ones to rare symbols,
and the average bits-per-symbol drops below the naive fixed-width
encoding (8 bits/byte here) whenever the input's symbol distribution is
non-uniform. It's still used today as the final stage of DEFLATE
(zip/gzip/PNG) after a dictionary pass like LZ77.

## Concept

```text
input
  |
  v
frequency table (count of each of the 256 byte values)
  |
  v
Huffman tree (greedily merge the two least-frequent nodes, repeat)
  |
  v
canonical codes (derived purely from code *lengths*, not the tree itself)
  |
  v
bit packing (MSB-first, padded to a byte boundary)
  |
  v
compressed file
```

## How it works

- **Tree construction** is O(n²) over at most 256 symbols (~65k
  comparisons worst case) via repeated linear scan for the two smallest
  frequencies — simpler to read than a binary heap, and just as fast at
  this alphabet size.
- **Canonical codes**: the compressed file stores only 256 code
  *lengths* (one byte per symbol, 0 = unused), never the tree and never
  the codes. `assign_canonical_codes()` is a pure function of those
  lengths, called identically by the encoder (to emit bits) and the
  decoder (to rebuild the same codes before decoding) — so the format
  never needs to serialize the tree structure itself.
- **Decoding** builds a small binary trie from the reconstructed codes
  and walks it one bit at a time, which is O(1) amortized per output
  byte rather than scanning all 256 symbols per bit.
- **File format** (268-byte header + bitstream): 4-byte magic `HUF1`,
  8-byte original size, 256 code-length bytes, then the packed bits.

## Implementation

- `include/huffman.h` — public API (file-based and in-memory).
- `src/huffman.c` — tree, canonical codes, bit I/O, decode trie.
- `src/main.c` — CLI.
- `src/bench.c` — the compression-ratio experiment (below).
- `tests/test_huffman.c` — round-trip tests: normal, empty, single-symbol,
  two-symbol, corrupt magic, truncated header, all 256 byte values.

## Example

```text
$ ./huffman compress examples/english.txt out.huf
Original:   1257 bytes
Compressed: 959 bytes
Ratio:      76.29%

$ ./huffman decompress out.huf restored.txt
$ diff examples/english.txt restored.txt   # (no output — identical)
```

## Experiments

**Which kinds of data compress well, and why does random data not?**
`src/bench.c` compresses four datasets under `examples/`: English prose,
a highly repetitive pattern, this lab's own `main.c` as a source-code
sample, and 6,400 bytes of `random.bin` (seeded PRNG output).

Real output from `make benchmark`:

```text
dataset        original     compressed      ratio encode(ms) decode(ms)
english            1257            959     76.29%      0.000      0.000
repetitive         6400           1068     16.69%      0.000      0.000
source_code        1450           1124     77.52%      0.000      0.000
random             6400           6665    104.14%      0.000      0.000
```

(Encode/decode times round to 0.000 ms at this file size — `clock()`'s
resolution is too coarse to resolve sub-100-microsecond work here; the
ratio numbers are the meaningful result at this scale.)

## Results

Repetitive data compresses best (16.69%) because one byte value
dominates the frequency table, earning it a very short code. English
text and source code land around 76-78% — real compression, but the
268-byte fixed header is a large fraction of these small (~1-1.5 KB)
files, so the ratio understates what Huffman achieves on the *content*
alone (on a multi-megabyte English corpus the header becomes
negligible and the ratio approaches the theoretical ~55-60% for
English). Random data is the standout: it comes out **larger** than the
input (104%) — with no skew in the byte-frequency distribution, every
symbol needs close to a full 8 bits, so the near-8-bit codes plus the
268-byte header can't help but add overhead. This is the concrete
demonstration of Shannon entropy: Huffman coding approaches the
entropy of the source, and uniformly random bytes have the maximum
possible entropy (8 bits/byte) — there is nothing left to compress.

## What I learned

Canonical Huffman codes are a much smaller file-format commitment than
storing the tree: 256 length bytes fully determine every code on the
decode side, because `assign_canonical_codes` is deterministic. That
also means the header cost is *fixed* (268 bytes) regardless of input
size — which is exactly what makes small-file compression ratios look
worse than the algorithm's real per-symbol efficiency.

## Limitations

- Fixed 268-byte header overhead makes tiny inputs (well under a
  kilobyte) compress poorly or even grow, independent of content.
- No run-length or dictionary stage (see `compression/lz77`) — Huffman
  alone can't exploit repeated *sequences*, only skewed single-byte
  frequency.
- Educational implementation: not hardened against adversarial input
  files, not optimized for very large files (whole file loaded into
  memory at once).

## Further experiments

- Measure the ratio on a much larger English corpus to see the header
  overhead disappear into the noise.
- Chain LZ77 before Huffman (like DEFLATE does) and compare the ratio
  to Huffman alone on the same repetitive dataset.
- Plot ratio vs. file size for the same repetitive pattern to find where
  the header stops mattering.
