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

`get(key)` checks the memtable first, then every SSTable from newest
to oldest — the first match wins, since a key can exist in several
SSTables with the newest value being current.

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
- **No SQL, no transactions, no query engine** — a single `put`/`get`
  key-value API, by design (CS-LAB.md §19).

## Implementation

- `src/lib.rs` — `Lsm`: open/put/get/flush/compact, plus the binary
  entry format shared by both the WAL and SSTables.
- `src/main.rs` — CLI, plus the compaction experiment (run with no
  arguments — see CONTRIBUTING.md's Rust-lab convention).
- `tests/tests.rs` — 9 tests: put/get, overwrite semantics, automatic
  flush at the memtable limit, reading from a flushed SSTable, newest-
  table-wins for a duplicate key, WAL crash recovery, compaction
  (merges + keeps latest value), and compacting an empty store.

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

**Does compaction pay for itself in read cost?** `cargo run --release`
(no args) writes 20 keys, each overwritten 15 times with a tiny
`memtable_limit` of 4 — producing 75 small SSTables, almost all of
them full of now-stale duplicate values. It then times 50 lookups of a
**missing** key (the worst case for a scan-every-table read path,
since it can't short-circuit on a hit) before and after `compact()`.

Real output:

```text
20 keys, each overwritten 15 times, memtable_limit=4
(measuring a MISSING key's lookup cost -- the worst case: every SSTable must be checked)

before compact: 75 SSTables, 75 total entries on disk, 50x missing-key get() took 250.230 ms
after  compact: 1 SSTable (20 live keys), 50x missing-key get() took 12.606 ms

missing-key lookups were 19.8x faster after compaction
```

## Results

Compaction made missing-key lookups **~20x faster** — collapsing 75
small files (most holding stale duplicates of just 20 real keys, ~300
total on-disk entries) into 1 file with the 20 live keys cuts both the
number of file opens per lookup and the total bytes scanned. This is
the concrete payoff for compaction's cost: without it, write-heavy
workloads accumulate SSTables indefinitely and every read (especially
a miss) gets slower over time; compaction trades some background I/O
now for bounded read cost later.

## What I learned

My first version of this experiment measured lookups of the 20 *known*
keys, and the speedup was a modest 1.3x — because every one of those
keys' current values lived in one of the most-recently-flushed
SSTables, which `get()` checks first (newest-to-oldest), so the
existing 75-table scan was already short-circuiting quickly for hits.
Switching to a *missing* key — which can never short-circuit and must
exhaust every SSTable — is what actually exercised the cost compaction
is supposed to fix, and the 20x number is the honest result.

## Limitations

- No bloom filters — a real LSM engine (and this repo's own
  `algorithms/bloom-filter`) would use one per SSTable specifically to
  make a missing-key lookup skip tables that provably don't contain
  the key, without reading them at all.
- No leveled/tiered compaction strategy — this lab does one
  "merge everything into one file" compaction, not RocksDB-style
  incremental leveled compaction.
- No deletes/tombstones (per CS-LAB.md §19's scope) — only `put`/`get`.
- Keys and values are both `String` for simplicity, not arbitrary bytes.
- SSTable lookups are a linear scan — no in-file index or sparse key
  offsets, so even a single SSTable's lookup cost is O(entries).

## Further experiments

- Add a bloom filter per SSTable (reusing `algorithms/bloom-filter`'s
  approach) and re-measure the missing-key benchmark — most SSTables
  should then be skippable without reading them at all.
- Add a sparse in-file index (e.g. an offset every 100 entries) and
  measure the improvement to a single SSTable's lookup cost.
- Trigger compaction automatically once SSTable count crosses a
  threshold, instead of only on an explicit `compact()` call, and
  measure the resulting read-latency curve over a long write workload.
