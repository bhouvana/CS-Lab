#!/usr/bin/env python3
"""Plain assert-based tests, no framework.

Parser tests run against a captured real gdb transcript (so they don't
need gdb installed); the integration test at the bottom runs the real
tool end-to-end and is skipped (not failed) if gdb isn't available --
this repo's own toolchain doesn't have it by default (see README)."""
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))
from walk_stack import parse_frames, render  # noqa: E402

# A real transcript captured from `gdb --batch -ex 'break bar' -ex run
# -ex 'info frame 0' -ex 'info frame 1' -ex 'info frame 2' ./nested`,
# frozen here so the parser can be tested without gdb installed.
SAMPLE_GDB_OUTPUT = """\
Breakpoint 1, bar (x=15) at src/nested.c:8
8	    int local_bar = x * 2;
Stack frame at 0x7fffffffd830:
 rip = 0x555555555158 in bar (src/nested.c:8); saved rip = 0x5555555551b9
 called by frame at 0x7fffffffd860
 source language c.
 Arglist at 0x7fffffffd820, args: x=15
 Locals at 0x7fffffffd820, Previous frame's sp is 0x7fffffffd830
 Saved registers:
  rbp at 0x7fffffffd820, rip at 0x7fffffffd828
Stack frame at 0x7fffffffd860:
 rip = 0x5555555551b9 in foo (src/nested.c:16); saved rip = 0x5555555551f1
 called by frame at 0x7fffffffd880, caller of frame at 0x7fffffffd830
 source language c.
 Arglist at 0x7fffffffd850, args: x=5
 Locals at 0x7fffffffd850, Previous frame's sp is 0x7fffffffd860
 Saved registers:
  rbp at 0x7fffffffd850, rip at 0x7fffffffd858
Stack frame at 0x7fffffffd880:
 rip = 0x5555555551f1 in main (src/nested.c:22); saved rip = 0x7ffff7c2a601
 caller of frame at 0x7fffffffd860
 source language c.
 Arglist at 0x7fffffffd870, args:
 Locals at 0x7fffffffd870, Previous frame's sp is 0x7fffffffd880
 Saved registers:
  rbp at 0x7fffffffd870, rip at 0x7fffffffd878
"""


def test_parse_frames_normal_case():
    frames = parse_frames(SAMPLE_GDB_OUTPUT)
    assert len(frames) == 3
    assert [f["func"] for f in frames] == ["bar", "foo", "main"]
    assert frames[0]["args"] == "x=15"
    assert frames[1]["args"] == "x=5"
    print("ok: parses 3 frames (bar, foo, main) from a real gdb transcript")


def test_parse_frames_extracts_saved_registers():
    frames = parse_frames(SAMPLE_GDB_OUTPUT)
    bar_frame = frames[0]
    assert bar_frame["rbp_slot"] == "0x7fffffffd820"
    assert bar_frame["rip_slot"] == "0x7fffffffd828"
    assert bar_frame["saved_rip"] == "0x5555555551b9"
    print("ok: extracts saved-RBP and saved-return-address slot addresses")


def test_render_contains_all_frames():
    frames = parse_frames(SAMPLE_GDB_OUTPUT)
    output = render(frames)
    assert "bar(x=15)" in output
    assert "foo(x=5)" in output
    assert "main()" in output
    print("ok: rendered diagram mentions all three frames")


def test_parse_frames_empty_input_edge_case():
    assert parse_frames("") == []
    print("ok: empty gdb output parses to zero frames, not a crash\n")


def test_gdb_integration_if_available():
    if shutil.which("gdb") is None:
        print("skip: gdb not installed in this environment (see README)")
        return
    binary = os.path.join(os.path.dirname(__file__), "..", "nested")
    if not os.path.exists(binary):
        print("skip: examples binary not built (run `make build` first)")
        return
    result = subprocess.run(
        [sys.executable, os.path.join(os.path.dirname(__file__), "..", "scripts", "walk_stack.py"), binary],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, result.stderr
    assert "bar(" in result.stdout and "foo(" in result.stdout and "main(" in result.stdout
    print("ok: end-to-end run against the real compiled binary")


if __name__ == "__main__":
    test_parse_frames_normal_case()
    test_parse_frames_extracts_saved_registers()
    test_render_contains_all_frames()
    test_parse_frames_empty_input_edge_case()
    test_gdb_integration_if_available()
    print("all tests passed")
