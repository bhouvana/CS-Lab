#!/usr/bin/env python3
"""Walks the REAL stack-frame chain of a running program via gdb (not
a fabricated diagram): breaks inside bar(), asks gdb for each frame's
base address, saved RBP slot, and saved return address, then renders
that as the STACK diagram from the project directive.

    binary + breakpoint
          |
          v
        gdb              runs the program, stops in bar(), inspects frames
          |
          v
    parsed frame data    (function, args, frame base, saved RBP/RIP)
          |
          v
      STACK diagram
"""
import re
import subprocess
import sys

FRAME_RE = re.compile(
    r"Stack frame at (0x[0-9a-f]+):\s*\n"
    r"\s*rip = (0x[0-9a-f]+) in (\w+) \([^)]*\);\s*saved rip = (0x[0-9a-f]+)\s*\n"
    r"(?:.*\n)*?"
    r"\s*Arglist at (0x[0-9a-f]+), args: ?(.*)\n"
    r".*\n"
    r"\s*Saved registers:\s*\n"
    r"\s*rbp at (0x[0-9a-f]+), rip at (0x[0-9a-f]+)",
)


def run_gdb(binary):
    cmd = [
        "gdb", "--batch",
        "-ex", "break bar",
        "-ex", "run",
        "-ex", "info frame 0",
        "-ex", "info frame 1",
        "-ex", "info frame 2",
        binary,
    ]
    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return result.stdout


def parse_frames(gdb_output):
    frames = []
    for m in FRAME_RE.finditer(gdb_output):
        base, rip, func, saved_rip, arglist_addr, args, rbp_slot, rip_slot = m.groups()
        frames.append({
            "base": base, "rip": rip, "func": func, "saved_rip": saved_rip,
            "arglist_addr": arglist_addr, "args": args,
            "rbp_slot": rbp_slot, "rip_slot": rip_slot,
        })
    return frames


def render(frames):
    lines = ["STACK", ""]
    for f in frames:
        lines.append(f"{f['func']}({f['args']})  [frame base {f['base']}]")
        lines.append(f" +-- locals/args at {f['arglist_addr']}")
        lines.append(f" +-- saved RBP stored at {f['rbp_slot']}  (points to the caller's frame)")
        lines.append(f" +-- return address stored at {f['rip_slot']}  (-> {f['saved_rip']}, in the caller)")
        lines.append("")
    return "\n".join(lines)


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <compiled-binary>", file=sys.stderr)
        return 2
    output = run_gdb(sys.argv[1])
    frames = parse_frames(output)
    if len(frames) != 3:
        print("error: expected exactly 3 stack frames (bar, foo, main); "
              f"parsed {len(frames)} -- gdb's output format may have changed.\n\n"
              f"raw gdb output:\n{output}", file=sys.stderr)
        return 1
    print(render(frames))
    return 0


if __name__ == "__main__":
    sys.exit(main())
