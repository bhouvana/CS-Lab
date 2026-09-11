# TCP Chat

**Linux/POSIX only** (POSIX sockets, `select()`). Built and verified
via WSL Ubuntu.

## What is this?

A multi-client chat server and client over raw POSIX sockets:
`socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`,
`recv()`, multiplexed with `select()` — no threads, no external
libraries.

```bash
./server 5555
./client 127.0.0.1 5555   # in another terminal, repeat for more clients
```

## Why does it matter?

This is networking at the level every higher-level abstraction (HTTP
servers, RPC frameworks, WebSocket libraries) is eventually built on.
Handling multiple simultaneous connections *without* threads — the
classic single-threaded `select()` event loop — is a foundational
systems-programming pattern, and it comes with a foundational gotcha
this lab ran straight into (see What I Learned).

## Concept

```text
server:
  socket() -> bind() -> listen()
        |
        v
  select() over {listener, every connected client}
        |
        +-- listener readable -> accept() a new client, broadcast "joined"
        |
        +-- client readable   -> recv(); 0 bytes = disconnect (broadcast "left"),
                                  else broadcast the message to everyone else

client:
  connect()
        |
        v
  select() over {stdin, socket} -- type and receive at the same time
```

## How it works

- **One `select()` loop, no threads**: the server tracks up to 32
  client file descriptors in a fixed array; each iteration rebuilds
  the `fd_set`, blocks in `select()`, then checks the listener and
  every client slot for readability.
- **Broadcast excludes the sender**: `broadcast(exclude_fd, msg, len)`
  sends to every connected client except one (the message's author,
  or `-1` to exclude nobody, e.g. for a disconnect notice).
- **Disconnect detection**: `recv()` returning `<= 0` means the peer
  closed (or an error occurred) — the slot is freed *before*
  broadcasting the "left" notice, so the just-closed fd is never
  itself a broadcast target.
- **`signal(SIGPIPE, SIG_IGN)`** — see What I Learned; this line is
  the fix for a real crash this lab hit while being built.

## Implementation

- `include/chat.h` — shared constants.
- `src/server.c` — the event loop.
- `src/server_poll.c` — the same protocol implemented with `poll()`.
- `src/server_queue.c` — a `poll()` server with non-blocking sockets and
  bounded per-client output queues.
- `src/client.c` — the interactive CLI (stdin + socket, multiplexed).
- `src/bench.c` — the broadcast-latency experiment.
- `tests/test_chat.c` — 5 integration tests, each with its own
  `fork()`+`exec()`'d server instance on its own port (driven by raw
  client sockets, not interactive stdin): join broadcast, message
  broadcast excludes sender, disconnect notification, a late-joining
  client sees no earlier private exchange, and the SIGPIPE regression.

## Example

```text
$ ./server 5555
chat server listening on port 5555
client 0 connected (fd=4)
client 1 connected (fd=5)
client 1 disconnected

# client A's terminal:
$ ./client 127.0.0.1 5555
connected to 127.0.0.1:5555 -- type messages, Ctrl-D to quit
*** client 1 joined ***
client 1: hello!
```

## Experiments

**How does broadcast latency scale with client count?** `src/bench.c`
connects N clients, has one send a message, and times how long until
the last of the other N-1 receives it (best of 3 trials, after a
warm-up round — see What I Learned).

Real output from `make benchmark`:

```text
clients       ms until all others received the message
2             0.083
4             0.076
8             0.115
16            0.151
32            0.173
```

## Results

Broadcast latency grows roughly with client count — expected, since
`broadcast()` is a simple `for` loop doing one `send()` per recipient,
so reaching N-1 clients is O(N) work done serially before the loop
returns. At 32 clients (0.173ms) it's still well under a millisecond
on localhost; this server's single-threaded, one-`send()`-at-a-time
design would need a real rewrite (non-blocking sends, or per-client
write queues) to stay fast at thousands of connections, but for the 32
clients this design targets, linear-but-small is a fine trade for the
simplicity of "no threads, no queues."

## Experiment implementations

The three follow-up experiments are now included as small, runnable
variants of this lab:

- `make experiments` builds `poll_server` from `src/server_poll.c`.
  It rebuilds a `struct pollfd` array on each iteration and handles
  `POLLIN`, `POLLHUP`, and `POLLERR` without changing the wire protocol.
- The same target builds `queue_server` from `src/server_queue.c`.
  Client sockets are non-blocking; broadcasts append to a 64 KiB
  per-client queue, and `POLLOUT` drains queued bytes. A client whose
  bounded queue fills is dropped instead of blocking every other client.
- The baseline `server` accepts `/nick <name>`. A valid name is
  announced to peers, and later messages use `<name>: <message>` instead
  of a numeric prefix. Names are limited to 31 bytes.

Validation on 2026-09-11: editor C diagnostics reported no errors for
the baseline server, both variants, or the integration test, and
`git diff --check -- networking/tcp-chat` passed. The documented Linux
commands (`make build`, `make test`, `make benchmark`, and `make
test-asan`) could not run because WSL reported `bash: not found` in this
environment. Therefore no new runtime or sanitizer numbers are claimed
for these variants; the latency table above remains the previously
recorded baseline benchmark.
## What I learned

Building `src/bench.c` crashed the server almost immediately once
multiple clients disconnected at the same time (exactly what closing
several benchmark client sockets in a tight loop does) — every
subsequent connection attempt then failed, because the server process
was simply gone. Tracing it with a debug build confirmed the sequence:
`select()` reports several clients readable-due-to-EOF in the same
wakeup; processing the first disconnect calls `broadcast()`, which can
still `send()` to a client slot that hasn't been processed *yet* in
this same pass but whose peer has *also* already closed — and `send()`
to an already-closed connection raises `SIGPIPE`, whose default action
silently terminates the whole process. One line,
`signal(SIGPIPE, SIG_IGN)`, fixed it (`send()` now just fails with
`EPIPE`, an ordinary recoverable error) — and
`test_simultaneous_disconnects_do_not_crash_server_regression` exists
specifically because I verified, by temporarily removing that one
line, that the test genuinely fails without it.

## Limitations

- 32-client cap (`MAX_CLIENTS` in `chat.h`) — a real server would grow
  its client table dynamically.
- No authentication or persistent usernames; names are in-memory only,
  and there is no message history for late joiners.
- `select()`, not `poll()`/`epoll()` — simpler and universally
  portable across POSIX systems, but `select()`'s O(highest fd) rebuild
  cost per iteration doesn't scale to very large client counts the way
  `epoll()` would.
- IPv4 only.
