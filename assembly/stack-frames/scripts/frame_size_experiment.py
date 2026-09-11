#!/usr/bin/env python3
"""Experiment: how does the number of local variables affect a
function's compiled stack frame size, and does the compiler round the
frame up to a fixed alignment?"""
import os
import re
import subprocess
import tempfile


def make_source(n):
    decls = "\n".join(f"    long v{i} = {i};" for i in range(n))
    uses = " + ".join(f"v{i}" for i in range(n)) if n else "0"
    return f"__attribute__((noinline)) long f(void) {{\n{decls}\n    return {uses};\n}}\nint main(void) {{ return (int)f(); }}\n"


def measure_frame_size(n):
    with tempfile.TemporaryDirectory() as d:
        src_path = os.path.join(d, "t.c")
        bin_path = os.path.join(d, "t")
        with open(src_path, "w") as f:
            f.write(make_source(n))
        subprocess.run(["gcc", "-O0", "-fno-omit-frame-pointer", src_path, "-o", bin_path], check=True)
        disasm = subprocess.run(
            ["objdump", "-d", bin_path, "-M", "att", "--disassemble=f"],
            capture_output=True, text=True, check=True,
        ).stdout
        match = re.search(r"sub\s+\$0x([0-9a-f]+),%rsp", disasm)
        return int(match.group(1), 16) if match else 0


def main():
    print("Experiment: local variable count vs. compiled stack frame size\n")
    print(f"{'locals':<10}{'bytes needed (locals*8)':<26}{'actual frame (sub $N,%rsp)'}")
    for n in [0, 1, 2, 4, 8, 16, 32]:
        needed = n * 8
        frame = measure_frame_size(n)
        print(f"{n:<10}{needed:<26}{frame}")


if __name__ == "__main__":
    main()
