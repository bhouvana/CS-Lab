import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))
from further_experiments import (  # noqa: E402
    backtrace_functions,
    frame_size,
    function_disassembly,
    saved_return_symbol,
)


def test_function_disassembly_isolates_requested_function():
    disasm = "00000000 <f>:\n   0: sub $0x20,%rsp\n\n00000010 <g>:\n"
    assert "<f>" in function_disassembly(disasm, "f")
    assert "<g>" not in function_disassembly(disasm, "f")
    assert frame_size(function_disassembly(disasm, "f")) == 0x20


def test_frame_size_supports_decimal_immediates():
    assert frame_size("sub $32,%rsp") == 32
    assert frame_size("sub $0xffffffffffffff80,%rsp") == 128
    assert frame_size("push %rbp") == 0


def test_backtrace_functions_preserves_gdb_frame_numbers():
    transcript = "#0  0x1 in bar ()\n#1  foo ()\n#2  main ()\n#3  __libc_start_main ()\n#4  _start ()\n"
    assert backtrace_functions(transcript) == [
        (0, "bar"),
        (1, "foo"),
        (2, "main"),
        (3, "__libc_start_main"),
        (4, "_start"),
    ]


def test_saved_return_symbol_extracts_startup_symbol():
    assert saved_return_symbol("__libc_start_call_main + 128 in section .text\n") == "__libc_start_call_main"


if __name__ == "__main__":
    test_function_disassembly_isolates_requested_function()
    test_frame_size_supports_decimal_immediates()
    test_backtrace_functions_preserves_gdb_frame_numbers()
    print("all further-experiment tests passed")