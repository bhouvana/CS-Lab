# LZ77

## What is this?

A small sliding-window LZ77 compressor/decompressor in C++, with real
binary output — fixed 4-byte tokens `(offset, length, next_symbol)`,
not printed codes.

```bash
./lz77 compress   input.txt  output.lz77
./lz77 decompress output.lz77 restored.txt
```

## Why does it matter?

LZ77 (Lempel-Ziv, 1977) is the dictionary-matching half of nearly
every general-purpose compressor in use today (DEFLATE/zip/gzip/PNG,
LZ4, Zstandard). Instead of exploiting skewed *symbol* frequencies
like Huffman coding (`compression/huffman`), it exploits skewed
*sequence* repetition: point backward at text you've already seen
instead of re-encoding it.

## Concept

```text
input
  |
  v
sliding window search    find the longest match to what comes next,
                          among the bytes already produced
  |
  v
(offset, length, next)   one token per match-or-literal
  |
  v
compressed bytes
```

```text
AAAAAAAABAAAAAAA
```
becomes 3 tokens: a literal `A`, a match reaching back 1 byte for
length 7 (covering `AAAAAAA`) plus literal `B`, and a match reaching
back 8 bytes for length 7 plus a final literal `A`.

## How it works

- **Tokens are fixed 4 bytes** (`offset`: 2 bytes, `length`: 1 byte,
  `next`: 1 byte) — deliberately simple, not bit-packed, per the
  project directive's explicit "keep the implementation intentionally
  simple, don't implement DEFLATE." `offset == 0` means "no match, this
  token is just the literal `next`."
- **Overlapping matches are allowed and load-bearing**: a match's
  `offset` can be smaller than its `length` (e.g. offset=1, length=50
  for a run of 50 identical bytes) — `lz77_decompress` reads
  `out[start + i]` one byte at a time as it appends, so it correctly
  regenerates a self-referential run rather than reading stale/
  out-of-bounds data.
- **No end-of-input special case**: the encoder always reserves at
  least 1 remaining byte for the trailing literal before searching for
  a match, so `next` is never missing — decode and encode never need a
  "match ran off the end of the input" branch anywhere.

## Implementation

- `include/lz77.hpp` / `src/lz77.cpp` — compress, decompress, and
  binary (de)serialization.
- `src/main.cpp` — CLI.
- `src/bench.cpp` — both experiments below.
- `tests/test_lz77.cpp` — 9 tests: the directive's own example,
  English text, empty/single-byte/two-byte edges, a long overlapping
  run, all 256 byte values, a corrupt token stream, and window-size
  enforcement.

## Example

```text
$ ./lz77 compress examples/repeat.txt out.lz77
Original:   16 bytes
Tokens:     3
Compressed: 12 bytes
Ratio:      75.00%
```

## Experiments

**1. Compression ratio by data type.** Repetitive data, English prose,
and this lab's own source code, all at the default 4096-byte window.

**2. Does a bigger window help, and only up to a point?** A 200-byte
block repeated after 300 bytes of unrelated filler (so the match is
exactly ~500 bytes back — far enough that small windows genuinely
cannot reach it).

Real output from `make benchmark`:

```text
Experiment 1: compression ratio by data type (default window=4096)

dataset                     original  tokens    compressed  ratio
repetitive (A's + B's)      2000      11        44          2.2%
English text                1201      358       1432        119.2%
this lab's own source code  3268      543       2172        66.5%

Experiment 2: a match ~500 bytes back -- does window size reaching
that far actually matter?

window        tokens    compressed
16            700       2800
64            80        320
256           80        320
512           55        220
1024          55        220
4096          55        220
```

## Results

**Repetitive data compresses to 2.2% of its original size** — 2000
bytes down to 44. **English text comes out 19% *larger*** than the
input (1201 -> 1432 bytes): with almost no long repeated sequences,
nearly every token is a lone literal, and this format's fixed 4-byte
cost per token (vs. 1 byte for the literal itself) is pure overhead in
that case. This is not a bug — it's the honest, direct answer to why
real compressors never ship LZ77 alone: DEFLATE follows it with Huffman
coding specifically to shrink that token stream back down, exactly the
`Huffman -> LZ77` connection this repository draws in `docs/
learning-path.md`.

Experiment 2 shows a clean threshold, not a gradual curve: window
sizes 16-256 all fail to reach the ~500-byte-back match (80-700
tokens, no improvement from 64 to 256), and window sizes 512 and above
all find it identically (55 tokens, no further improvement from 512 to
4096). The window only needs to be *big enough*; past that, making it
bigger buys nothing for this data.

## What I learned

My first version of experiment 2 reused experiment 1's repetitive test
string, and every window size from 16 to 16384 produced the identical
result — the pattern's actual repeat distance was only 16 bytes, so
even the smallest window already covered it completely, and the
experiment silently measured nothing. Building a test case with a
*specific, deliberate* match distance (~500 bytes) was necessary to
actually observe the effect the experiment claimed to measure.

## Limitations

- Fixed 4-byte tokens waste space on non-repetitive data — see Results.
  A real implementation would follow this with entropy coding (like
  DEFLATE does) or use a variable-width/bit-packed token format.
- Naive O(window x lookahead) match search per position (no hash
  chains or suffix structures) — fine at this lab's scale, far too
  slow for large files.
- `offset`/`length` fit in 16/8 bits, capping window size at 65535 and
  match length at 255.

## Further experiments

- Chain this lab's output through `compression/huffman` on the
  serialized token stream and measure whether that recovers real
  compression on the English-text case that expanded here.
- Replace the fixed 4-byte token with a 1-bit flag + variable encoding
  (literal: 1 bit + 8 bits; match: 1 bit + offset + length) and
  re-measure the English-text ratio.
- Add hash-chain-based match finding and measure the compression-speed
  improvement on a multi-megabyte file.
