#!/usr/bin/env python3
"""Plain assert-based tests, no framework."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "src"))
from pipeline import Instruction, parse_program, simulate, stats  # noqa: E402


def test_no_hazard_normal_case():
    instrs = parse_program(["ADD R1, R2, R3", "SUB R4, R5, R6"])  # independent, no shared registers
    stage_start, total_cycles = simulate(instrs, forwarding=True)
    assert total_cycles == 6  # n=2, ideal = 2+4 = 6, no stalls possible
    s = stats(instrs, stage_start, total_cycles)
    assert s["stalls"] == 0
    print("ok: independent instructions have zero stalls")


def test_raw_hazard_without_forwarding_stalls_until_after_wb():
    instrs = parse_program(["ADD R1, R2, R3", "SUB R4, R1, R5"])
    stage_start, total_cycles = simulate(instrs, forwarding=False)
    # ADD: IF1 ID2 EX3 MEM4 WB5. SUB needs R1, not ready until WB+1=6.
    assert stage_start[1]["EX"] == 6
    print("ok: RAW hazard without forwarding stalls until after producer's WB")


def test_raw_hazard_with_forwarding_needs_no_stall_for_alu_producer():
    instrs = parse_program(["ADD R1, R2, R3", "SUB R4, R1, R5"])
    stage_start, total_cycles = simulate(instrs, forwarding=True)
    s = stats(instrs, stage_start, total_cycles)
    assert s["stalls"] == 0  # EX/MEM forwarding fully hides an ALU-to-ALU RAW hazard
    print("ok: forwarding eliminates the ALU-to-ALU RAW hazard entirely")


def test_load_use_hazard_needs_one_stall_even_with_forwarding():
    instrs = parse_program(["LOAD R1, 0(R2)", "ADD R3, R1, R4"])
    stage_start, total_cycles = simulate(instrs, forwarding=True)
    s = stats(instrs, stage_start, total_cycles)
    assert s["stalls"] == 1  # the classic load-use hazard: 1 stall even with forwarding
    print("ok: load-use hazard costs exactly 1 stall cycle even with forwarding")


def test_directive_example_program_regression():
    instrs = parse_program(open("examples/program.asm").readlines())
    with_fwd, cycles_fwd = simulate(instrs, forwarding=True)
    without_fwd, cycles_no_fwd = simulate(instrs, forwarding=False)
    assert cycles_fwd == 9 and cycles_no_fwd == 12
    assert stats(instrs, with_fwd, cycles_fwd)["stalls"] == 1
    assert stats(instrs, without_fwd, cycles_no_fwd)["stalls"] == 4
    print("ok: bundled example program -> 9 cycles (forwarding) vs 12 (no forwarding)")


def test_single_instruction_edge_case():
    instrs = parse_program(["ADD R1, R2, R3"])
    stage_start, total_cycles = simulate(instrs, forwarding=True)
    assert total_cycles == 5  # IF ID EX MEM WB, one cycle each
    print("ok: single instruction takes exactly 5 cycles")


def test_empty_program_edge_case():
    instrs = parse_program([])
    stage_start, total_cycles = simulate(instrs, forwarding=True)
    assert total_cycles == 0
    print("ok: empty program handled without crashing")


def test_load_instruction_parses_base_register():
    instr = Instruction("LOAD R6, 8(R2)")
    assert instr.dest == "R6"
    assert instr.srcs == ["R2"]
    print("ok: LOAD Rd, offset(Rs) parses the base register as the only source")


def test_branch_stalls_younger_fetch_until_ex():
    instrs = parse_program(["BEQ R1, R2, target", "ADD R3, R4, R5"])
    stage_start, total_cycles = simulate(instrs, forwarding=True)
    assert stage_start[0]["EX"] == 3
    assert stage_start[1]["IF"] == 4
    assert stats(instrs, stage_start, total_cycles)["stalls"] == 2
    print("ok: branch resolution in EX delays younger fetch")


def test_comment_and_blank_lines_ignored_invalid_case():
    instrs = parse_program(["# a comment", "", "ADD R1, R2, R3", "   "])
    assert len(instrs) == 1
    print("ok: comments and blank lines are skipped")


if __name__ == "__main__":
    test_no_hazard_normal_case()
    test_raw_hazard_without_forwarding_stalls_until_after_wb()
    test_raw_hazard_with_forwarding_needs_no_stall_for_alu_producer()
    test_load_use_hazard_needs_one_stall_even_with_forwarding()
    test_directive_example_program_regression()
    test_single_instruction_edge_case()
    test_empty_program_edge_case()
    test_load_instruction_parses_base_register()
    test_branch_stalls_younger_fetch_until_ex()
    test_comment_and_blank_lines_ignored_invalid_case()
    print("all tests passed")
