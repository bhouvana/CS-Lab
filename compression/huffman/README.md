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
english            1257            959     76.29%      0.016      0.023
repetitive         6400           1068     16.69%      0.030      0.019
source_code        1450           1124     77.52%      0.018      0.019
random             6400           6665    104.14%      0.192      0.097
```

**Does the header disappear into the noise on a much larger English
corpus?** Same `examples/english.txt` text, repeated in memory to 10x/
100x/1000x its size (the frequency distribution stays representative
of real English either way — it's the same underlying text):

```text
size                 original     compressed      ratio
1x (baseline)            1257            959     76.29%
10x                     12570           7175     57.08%
100x                   125700          69331     55.16%
1000x                 1257000         690893     54.96%
```

**Ratio vs. file size, same repetitive pattern** (`"AAAAAAAAB"` repeated
to each size — where does the fixed header stop mattering?):

```text
size                 original     compressed      ratio
100 bytes                 100            281    281.00%
1000 bytes               1000            393     39.30%
10000 bytes             10000           1518     15.18%
100000 bytes           100000          12768     12.77%
1000000 bytes         1000000         125268     12.53%
```

**Chaining LZ77 before Huffman, like DEFLATE does** (manual measurement
via `compression/lz77`'s and this lab's own CLIs on the same
`examples/repetitive.txt`, and on a 320,000-byte version of it —
50 copies concatenated — to see whether scale changes the answer):

```text
6,400-byte repetitive.txt:
  Huffman alone:        1,068 bytes (16.69%)
  LZ77 alone:              112 bytes (1.75%)
  LZ77 -> Huffman:         312 bytes (278.57% of the 112-byte LZ77 output)
  -> chaining HURTS here: Huffman's 268-byte header alone is more than
     double the entire LZ77 output it's compressing.

320,000-byte repetitive.txt (50 copies):
  Huffman alone:        40,268 bytes (12.58%)
  LZ77 alone:            5,012 bytes (1.57%)
  LZ77 -> Huffman:        1,690 bytes (33.72% of the 5,012-byte LZ77 output)
  -> chaining HELPS a lot here: final size is ~3x smaller than LZ77
     alone, ~24x smaller than Huffman alone.
```

## Results

Repetitive data compresses best (16.69%) because one byte value
dominates the frequency table, earning it a very short code. English
text and source code land around 76-78% — real compression, but the
268-byte fixed header is a large fraction of these small (~1-1.5 KB)
files, so the ratio understates what Huffman achieves on the *content*
alone. Random data is the standout: it comes out **larger** than the
input (104%) — with no skew in the byte-frequency distribution, every
symbol needs close to a full 8 bits, so the near-8-bit codes plus the
268-byte header can't help but add overhead. This is the concrete
demonstration of Shannon entropy: Huffman coding approaches the
entropy of the source, and uniformly random bytes have the maximum
possible entropy (8 bits/byte) — there is nothing left to compress.

**The larger-corpus and ratio-vs-size experiments confirm the header
theory directly, on two different datasets.** English text's ratio
drops from 76.29% at 1x to 54.96% at 1000x, converging toward the
theoretical ~55% for English (source: standard information-theory
estimates of English entropy) as the fixed 268-byte header becomes
negligible against the growing content. The repetitive pattern shows
the same convergence even more sharply — 281% (grows!) at 100 bytes,
down to 12.53% at 1,000,000 bytes — because at 100 bytes the header
*is* almost the entire output.

**Chaining LZ77 before Huffman is a genuine two-sided result, not a
clean "chaining is better."** At small scale it actively hurts (312
bytes vs. 112 for LZ77 alone) for exactly the header-overhead reason
above: Huffman's fixed 268-byte cost dwarfs a 112-byte input regardless
of any further redundancy. At larger scale it wins decisively (1,690
bytes vs. 5,012 for LZ77 alone) because the LZ77 token stream itself
still has real byte-frequency skew Huffman can exploit, and by then the
header is a small fraction of a 5KB+ intermediate stream. This is
exactly why real DEFLATE always follows LZ77 with Huffman coding on
real-world-sized data — the combination beats either alone once inputs
are large enough for the fixed cost to amortize, but "always chain
them" isn't universally true at every size, which a single data point
wouldn't have shown.

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

