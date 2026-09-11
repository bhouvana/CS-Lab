#!/usr/bin/env python3
"""Run the three stack-frame experiments listed in the lab README."""
import os
import re
import shutil
import subprocess
import tempfile


def function_disassembly(disasm, function):
    marker = re.search(rf"^[0-9a-f]+ <{re.escape(function)}>:\n", disasm, re.MULTILINE)
    if marker is None:
        return ""
    following = re.search(r"\n[0-9a-f]+ <[^>]+>:\n", disasm[marker.end():])
    end = marker.end() + following.start() if following else len(disasm)
    return disasm[marker.start():end]


def compile_source(source, flags):
    directory = tempfile.TemporaryDirectory()
    source_path = os.path.join(directory.name, "experiment.c")
    binary_path = os.path.join(directory.name, "experiment.exe")
    with open(source_path, "w") as source_file:
        source_file.write(source)
    subprocess.run(["gcc", *flags, source_path, "-o", binary_path], check=True)
    return directory, binary_path


def disassemble(binary_path, function):
    output = subprocess.run(
        ["objdump", "-d", binary_path],
        capture_output=True, text=True, check=True,
    ).stdout
    return function_disassembly(output, function)


def frame_size(disasm):
    match = re.search(r"sub\s+\$(?:0x([0-9a-f]+)|([0-9]+)),%rsp", disasm)
    if match is None:
        return 0
    if match.group(2):
        return int(match.group(2))
    value = int(match.group(1), 16)
    return abs(value - (1 << 64)) if value > (1 << 63) else value


def bar_source(local_count):
    declarations = "\n".join(
        f"    volatile long local_{index} = seed + {index};"
        for index in range(local_count)
    )
    additions = "\n".join(
        f"    total += local_{index};" for index in range(local_count)
    )
    return f"""#include <stdio.h>
__attribute__((noinline)) long bar(long seed) {{
{declarations}
    long total = seed;
{additions}
    printf(\"%ld\\n\", total);
    return total;
}}
int main(void) {{ return (int)bar(1); }}
"""


def measure_bar_frames(local_counts):
    results = []
    for local_count in local_counts:
        directory, binary = compile_source(
            bar_source(local_count), ["-O0", "-g", "-fno-omit-frame-pointer"]
        )
        try:
            results.append((local_count, frame_size(disassemble(binary, "bar"))))
        finally:
            directory.cleanup()
    return results


def run_gdb(binary_path, commands):
    command = ["gdb", "--batch", "-nx"]
    for item in commands:
        command.extend(["-ex", item])
    command.append(binary_path)
    result = subprocess.run(command, capture_output=True, text=True, check=True)
    return result.stdout + result.stderr


def backtrace_functions(transcript):
    functions = {}
    for line in transcript.splitlines():
        match = re.match(r"^#(\d+)\s+(?:0x[0-9a-f]+ in )?([A-Za-z_][\w@.$]*)", line)
        if match:
            functions[int(match.group(1))] = match.group(2)
    return sorted(functions.items())


def deep_backtrace(binary_path, limit=12):
    transcript = run_gdb(
        binary_path,
        [
            "set pagination off",
            "break bar",
            "run",
            f"bt {limit}",
            "frame 2",
            "x/gx $rbp+8",
            "info symbol *(void**)($rbp+8)",
        ],
    )
    return backtrace_functions(transcript), transcript


def saved_return_symbol(transcript):
    for line in transcript.splitlines():
        match = re.match(r"^([A-Za-z_][\w@.$]*)\s+(?:\+|in section)", line)
        if match:
            return match.group(1)
    return ""


def optimization_comparison(source_path):
    results = []
    for label, flags in [
        ("O0 + frame pointer", ["-O0", "-g", "-fno-omit-frame-pointer"]),
        ("O2 default", ["-O2", "-g"]),
    ]:
        with open(source_path) as source_file:
            source = source_file.read()
        directory, binary = compile_source(source, flags)
        try:
            bar_disasm = disassemble(binary, "bar")
            functions, transcript = deep_backtrace(binary, 6)
            results.append({
                "label": label,
                "has_frame_pointer_prologue": "push   %rbp" in bar_disasm and "mov    %rsp,%rbp" in bar_disasm,
                "functions": functions,
                "transcript": transcript,
            })
        finally:
            directory.cleanup()
    return results


def main():
    print("Experiment 1: bar() locals with a printf call")
    print("locals    actual frame (sub $N,%rsp)")
    for local_count, size in measure_bar_frames([0, 1, 2, 4, 8, 16, 32]):
        print(f"{local_count:<10}{size}")

    if shutil.which("gdb") is None:
        print("\nExperiment 2: skipped (gdb is not installed)")
    else:
        print("\nExperiment 2: frames below main()")
        source_path = os.path.join(os.path.dirname(__file__), "..", "src", "nested.c")
        with open(source_path) as source_file:
            directory, binary = compile_source(
                source_file.read(), ["-O0", "-g", "-fno-omit-frame-pointer"]
            )
        try:
            functions, transcript = deep_backtrace(binary)
            chain = " -> ".join(name for _, name in functions) or "(no frames returned by gdb)"
            caller = saved_return_symbol(transcript)
            print(chain + (f" -> {caller} (main's saved return address)" if caller else ""))
        finally:
            directory.cleanup()

    print("\nExperiment 3: gdb and frame pointers at O2")
    for result in optimization_comparison(os.path.join(os.path.dirname(__file__), "..", "src", "nested.c")):
        print(f"{result['label']}: frame-pointer prologue={result['has_frame_pointer_prologue']}")
        print("  backtrace: " + (" -> ".join(name for _, name in result["functions"]) or "(no frames returned by gdb)"))


if __name__ == "__main__":
    main()