# Engineering Quality Scorecard

**PASS** verified, no known gap · **PARTIAL** works, with a stated gap ·
**N/A** doesn't apply to this lab's design. No numerical scores -- see
`docs/engineering-audit.md` for the evidence behind each cell and
`docs/production-readiness.md` for what these ratings do and don't claim.

| Lab | Correctness | Testing | Error Handling | Portability¹ | Determinism | Documentation | Benchmarking | Memory Safety² |
|---|---|---|---|---|---|---|---|---|
| SAT Solver | PASS | PASS | PASS | PASS | PASS (seed 42) | PASS | PASS | PASS (ASan+UBSan) |
| Graph Algorithms | PASS | PASS | PASS | PASS | PASS | PASS | N/A (no bench target) | PARTIAL (not sanitizer-checked; no known issue, just unverified) |
| Bloom Filter | PASS | PASS | PASS (`assert!`) | PASS | PASS | PASS | PASS | PASS (Rust + clippy clean) |
| Tiny Language | PASS | PASS | PASS (line-numbered `Result` errors) | PASS | PASS | PASS | N/A (interpreter demo) | PASS (Rust + clippy clean) |
| Bytecode VM | PASS | PASS (+ 20k-case fuzz) | PASS | PASS | PASS (fuzz seed) | PASS | PASS | PASS (ASan+UBSan) |
| Huffman | PASS | PASS | PASS | PASS | PASS | PASS | PASS (fixed this pass) | PASS (ASan+UBSan) |
| LZ77 | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS (ASan+UBSan) |
| SHA-256 | PASS (matches `sha256sum` + NIST vector) | PASS | PASS | PASS | PASS | PASS | PASS | PASS (ASan+UBSan) |
| Cache Simulator | PASS | PASS | PASS | PASS | PASS | PASS | N/A (trace-driven) | PARTIAL (not sanitizer-checked) |
| Branch Predictor | PASS | PASS | PASS | PASS | PASS (seed 42) | PASS | PASS | PARTIAL (not sanitizer-checked) |
| Pipeline Simulator | PASS | PASS | PASS | PASS | PASS | PASS | PASS | N/A (Python) |
| Garbage Collector | PASS | PASS (incl. 200k-deep chain) | N/A (internal API) | PASS | PASS | PASS | PASS | PASS (ASan+UBSan) |
| Tiny LSM | PASS | PASS | PASS (`io::Result`) | PASS | PASS | PASS | PASS | PASS (Rust + clippy clean) |
| Constant-Time Compare | PASS | PASS | N/A (fixed-size comparison, no invalid input) | PASS | PARTIAL³ | PASS (explicit noise caveat) | PASS | PASS (ASan+UBSan) |
| Allocator | PASS | PASS | PASS | PASS | PASS | PASS | PASS | PASS (ASan+UBSan) |
| Shell | N/A | N/A | N/A | N/A | N/A | PASS (states "not implemented" plainly) | N/A | N/A |
| TCP Chat | PASS | PASS (incl. SIGPIPE + isolation regressions) | PASS | PASS (Linux; clean `skip:` elsewhere) | PASS | PASS | PASS | PASS (ASan+UBSan) |
| Calling Convention | PASS | PASS | N/A (no invalid-input surface -- pure ABI demo) | PASS (Linux x86-64; clean `skip:` elsewhere) | PASS | PASS | PASS | PARTIAL (hand-written `.S` isn't ASan-instrumented; the C caller is safe by inspection) |
| Stack Frames | PASS | PASS (parser + end-to-end) | PASS (empty-input case) | PASS (Linux x86-64; clean `skip:` elsewhere) | PASS | PASS | N/A | PARTIAL (same as above) |
| Syscall Lab | PASS | PASS (incl. `-EBADF` paths) | PASS | PASS (Linux x86-64; clean `skip:` elsewhere) | PASS | PASS | PASS | PARTIAL (same as above) |

¹ "Portability" here means *behaves correctly on every platform it claims to
support* -- for the Linux-only labs, that includes printing a clean `skip:`
line and exiting 0 on Windows, not literally running there. See
`docs/engineering-audit.md`'s two tables for which labs are which.

² "Memory Safety" for Rust labs means the language's own guarantees plus a
clean `cargo clippy`, not a C-style sanitizer run (there's no unsafe code to
sanitize). For the 9 C/C++ labs on CS-LAB.md §12's priority list it means a
clean ASan+UBSan run, verified this pass. For the rest it's PARTIAL, not
FAIL -- they build warning-free under `-Wall -Wextra -Wpedantic` and their
tests pass; they just haven't had a dedicated sanitizer pass. The 3 assembly
labs' hand-written `.S` files can't be ASan-instrumented at all (only their
C wrapper code could be, and that wrapper does essentially no memory
manipulation of its own).

³ Constant-time compare's *algorithm* is deterministic; its *measured
timing* is not, by nature of measuring wall-clock time on a general-purpose
OS with scheduler noise, cache effects, and frequency scaling. The lab's own
README says so explicitly: timing measurements here don't constitute a
formal proof of constant-time behavior. That caveat is itself the
correct, hardened answer -- overclaiming determinism for a timing side-
channel demo would be the actual quality defect.
