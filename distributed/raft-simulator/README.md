# Raft Simulator

**This is a simulator, not a production distributed database.**

## What is this?

A deterministic, single-process simulation of Raft consensus: leader
election, terms, heartbeats, and basic log replication across a
simulated cluster of nodes.

```bash
cargo run --bin raft -- --nodes 5
```

## Why does it matter?

Raft is the consensus algorithm behind etcd, Consul, CockroachDB, and
countless others — the mechanism that lets a cluster of machines agree
on a single sequence of events even when some of them fail. This lab
implements the actual state machine (not a diagram of it): term
comparisons, vote-granting rules, the "at least as up to date" log
check, and the majority-commit rule, all runnable and testable.

## Concept

```text
nodes (Follower / Candidate / Leader)
         |
         v
       tick()          election timeouts fire, leaders send heartbeats
         |
         v
  Raft state machine    RequestVote / AppendEntries handled as direct,
                         synchronous calls -- deterministic "messages"
                         with no real network transport (CS-LAB.md §21)
         |
         v
   leader / log state
```

```text
tick 150: node 0 times out, starts election for term 1
tick 150: node 0 becomes LEADER for term 1
tick 250: node 0 killed
tick 400: node 1 times out, starts election for term 2
tick 400: node 1 becomes LEADER for term 2
```

## How it works

- **"Sending a message" is a direct function call that returns its
  reply immediately**, in the same tick — the literal meaning of
  "simulated messages, no network transport" here. Every Raft rule is
  still fully implemented; only the transport is elided.
- **Deterministic, staggered election timeouts** (`150 + node_id * 10`
  ticks) instead of randomized ones — the standard Raft paper
  randomizes timeouts specifically to avoid split votes, but that
  makes outcomes non-reproducible. Staggering achieves the same
  "someone times out first" property deterministically, at the cost of
  losing the ability to observe split-vote dynamics (see Limitations).
- **Leader election**: a node whose timeout fires increments its term,
  votes for itself, and requests votes from every other alive node;
  each voter grants at most one vote per term, and only if the
  candidate's log is at least as up to date as its own.
- **Log replication**: `submit()` appends to the leader's log; the
  next heartbeat carries new entries to followers via `AppendEntries`
  (with the log-consistency check via `prev_log_index`/`prev_log_term`
  standard to Raft); an entry commits once a majority (leader
  included) has replicated it *and* it belongs to the leader's current
  term (the safety rule that prevents indirectly committing an
  earlier-term entry via replication count alone).

## Implementation

- `src/lib.rs` — `Cluster`, `Node`, the whole Raft state machine.
- `src/main.rs` — CLI: the leader-failure scenario, then the recovery
  experiment.
- `tests/tests.rs` — 8 tests: a leader emerges, exactly one leader
  exists at a time, killing the leader triggers a new election with a
  higher term, a submitted command commits and replicates to a
  majority, submitting with no leader fails, a minority partition
  (2 of 5 alive) can never elect a leader, and a revived node rejoins
  cleanly as a follower.

## Example

```text
$ cargo run --bin raft -- --nodes 5
Raft simulator: 5 nodes

--- killing the leader (node 0) ---

tick 150: node 0 times out, starts election for term 1
tick 150: node 0 becomes LEADER for term 1
tick 200: leader 0 appends 'x=1' at log index 1
tick 200: leader 0 appends 'x=2' at log index 2
tick 220: leader 0 commits log index 2
tick 250: node 0 killed
tick 400: node 1 times out, starts election for term 2
tick 400: node 1 becomes LEADER for term 2

final state: node 1 is leader (term 2), commit_index 2
```

## Experiments

**What happens when the leader fails, repeatedly?** `src/main.rs`
starts a 5-node cluster, and after each leader election, kills the
current leader and waits for the next one — up to the point where one
more failure would drop the cluster below a majority (2 failures
tolerated out of 5 nodes).

Real output:

```text
A 5-node cluster tolerates up to 2 failures before losing majority (3/5).
event               ticks since the previous event
leader elected      150
leader elected      160
leader elected      170
```

## Results

Each recovery takes roughly the same number of ticks (150, then 160,
then 170) regardless of how many prior failures the cluster has
already survived — the cluster doesn't get slower or more fragile
after a failure, as long as a majority remains alive. The small,
steady increase (150 -> 160 -> 170) is a direct, visible consequence
of this simulator's staggered-timeout design: after killing node k,
the next-lowest-ID survivor's deadline is always exactly 10 ticks
later than the previous leader's was, since deadlines are assigned
`150 + id * 10`.

## What I learned

My first version of the "experiment" swept cluster size (3, 5, 7, 9,
11 nodes) expecting election time to grow with cluster size — every
single size reported exactly 150 ticks. That's not a bug: with fully
deterministic, ID-staggered timeouts, node 0 *always* wins the timeout
race first, regardless of how many other nodes exist, so cluster size
was never actually wired to anything the experiment measured. Swapping
to "how does recovery time change across *repeated* failures" (where
the staggered design actually does produce different, explainable
numbers) is what turned this into a real experiment instead of an
accidentally-constant one — the same lesson as `tiny-lsm`'s and
`branch-predictor`'s "what I learned" sections, from a different angle:
know what your specific design choices make a benchmark sensitive to
before trusting its output.

## Limitations

- **Deterministic timeouts, not randomized ones** — real Raft
  deliberately randomizes election timeouts to make split votes rare;
  this simulator's staggered timeouts make split votes essentially
  impossible to observe, trading that realism for full reproducibility.
- **No network partitions** (nodes are only "alive" or "killed," not
  "can talk to some nodes but not others") — a harder and more
  interesting failure mode real Raft implementations must handle.
- **No log compaction/snapshotting** — logs grow unboundedly.
- **No persistence** — a "killed" node's state (that it hasn't lost,
  since `kill` doesn't clear it) survives in memory only because this
  is one process; a real crash would need to reload persisted state.

## Further experiments

- Add randomized election timeouts (seeded, for reproducible-but-random
  runs) and measure how often split votes actually occur at different
  cluster sizes.
- Simulate a network partition (some nodes can message each other but
  not others) instead of only total node failure, and observe what
  happens when no side has a majority.
- Add log compaction and measure log size over a long run of
  `submit()` calls with and without it.
