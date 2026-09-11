# Tiny LSM

## What is this?

A miniature log-structured merge (LSM) storage engine in Rust:
`put`/`get` over a write-ahead log, an in-memory sorted memtable, and
on-disk sorted SSTables, with a real merge/compaction step.

```bash
cargo run --bin lsm -- put ./data name Claude
cargo run --bin lsm -- get ./data name
cargo run --bin lsm -- compact ./data
```

## Why does it matter?

Modern storage engines (RocksDB, Cassandra, LevelDB, and what's under
the hood of many "NoSQL" databases) almost universally choose
**append, sort, merge** over "find the record on disk and modify it in
place." This lab is small enough to make *why* concrete: sequential
writes are fast, in-place random writes on real storage are not, and
the price you pay for that speed is a periodic cleanup pass
(compaction) — this lab measures that price and that payoff directly.

## Concept

```text
put(key, value)
      |
      v
     WAL              fsync'd before put() returns -- durability first
      |
      v
   memtable           an in-memory BTreeMap (sorted by key)
      |
      v  (once memtable_limit entries)
   flush
      |
      v
   SSTable            a new sorted, immutable file on disk
      |
      v  (compact())
    merge              all SSTables -> one, duplicates resolved by recency
```

`get(key)` checks the memtable first, then SSTables from newest to
oldest — the first match wins, since a key can exist in several
SSTables with the newest value being current. Each SSTable has a
Bloom filter to skip definite misses and a sparse offset index to
avoid scanning its entire sorted file.

## How it works

- **Durability**: `put()` writes to the WAL and calls `sync_all()`
  *before* touching the memtable, so a crash between "value written"
  and "value flushed to an SSTable" never loses data — `Lsm::open()`
  replays any pending WAL entries back into the memtable on startup.
  `wal_replay_recovers_unflushed_writes_after_a_crash` tests exactly
  this, by dropping an `Lsm` without an explicit flush and reopening.
- **Flush**: once the memtable reaches `memtable_limit` entries, it's
  written out as one new sorted SSTable file and cleared; the WAL is
  truncated since its only job was protecting data that's now safely
  on disk in sorted form.
- **Compaction**: reads every SSTable, oldest to newest, into one
  `BTreeMap` (so a later table's value for a duplicate key naturally
  overwrites an earlier one), then writes that single merged map out
  as the new (and only) SSTable, deleting the old files.
- **Automatic compaction**: setting `auto_compact_threshold` makes a
  flush compact the store when the SSTable count crosses that threshold.
- **No SQL, no transactions, no query engine** — a single `put`/`get`
  key-value API, by design (CS-LAB.md §19).

## Implementation

- `src/lib.rs` — `Lsm`: open/put/get/flush/compact, per-SSTable Bloom
  filters and sparse indexes, plus the binary entry format shared by
  both the WAL and SSTables.
- `src/main.rs` — CLI, plus the three experiments (run with no
  arguments — see CONTRIBUTING.md's Rust-lab convention).
- `tests/tests.rs` — 11 tests: put/get, overwrite semantics, automatic
  flush at the memtable limit, reading from a flushed SSTable, newest-
  table-wins for a duplicate key, WAL crash recovery, compaction
  (merges + keeps latest value), compacting an empty store, indexed
  reads, and threshold-triggered compaction.

## Example

```text
$ cargo run --bin lsm -- put ./data name Claude
put name = Claude
$ cargo run --bin lsm -- put ./data lab tiny-lsm
put lab = tiny-lsm
$ cargo run --bin lsm -- get ./data name
Claude
$ cargo run --bin lsm -- stats ./data
memtable entries: 2
SSTables:         0
```

## Experiments

Running `cargo run --release` (no args) runs all three experiments.
Missing-key lookups are used because they cannot short-circuit on a
hit and therefore expose the cost of reading unnecessary data.

Real output:

```text
Experiment 1: Bloom filters for missing-key reads
75 SSTables, 50 missing-key lookups
sparse index without Bloom filter: 254.403 ms
Bloom filter enabled:              0.481 ms
Bloom-filter speedup: 529.5x

Experiment 2: sparse in-file index
1 SSTable, 1000 entries, 30 missing-key lookups
linear scan: 17.981 ms
sparse index (one offset per 100 entries): 3.028 ms
sparse-index speedup: 5.9x

Experiment 3: automatic compaction threshold
threshold=8 SSTables, 20 overwritten keys per round
round  SSTables  20x missing-key gets (ms)
    1         1                        0.006
    8         8                        0.034
    9         1                        0.006
   16         8                        0.027
   24         8                        0.036
   40         8                        0.033
```

## Results and lessons

The Bloom filter was the largest win in this workload: it skipped all
75 definite misses and made lookups 529.5x faster than the sparse scan
without a filter. The sparse index made a single-table miss 5.9x faster
by starting near the target's sorted-file position instead of offset
zero. Automatic compaction kept the table count bounded at 8; crossing
the threshold compacted 9 tables back to 1, keeping missing-key latency
near 0.03 ms over 40 overwrite rounds instead of allowing an unbounded
table scan.

## Limitations

- No leveled/tiered compaction strategy — this lab does one
  "merge everything into one file" compaction, not RocksDB-style
  incremental leveled compaction.
- No deletes/tombstones (per CS-LAB.md §19's scope) — only `put`/`get`.
- Keys and values are both `String` for simplicity, not arbitrary bytes.
- Bloom filters and sparse indexes are rebuilt in memory when opening
  an SSTable; they are not persisted as separate metadata files.
- The sparse index uses one offset every 100 entries, so a lookup still
  scans a bounded tail of the table rather than doing a binary search.
