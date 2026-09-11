#!/usr/bin/env python3
"""Experiment: how much does forwarding reduce stalls, and does that
depend on how hazard-dense the code is?"""
import sys

from pipeline import Instruction, simulate, stats


def make_dependent_chain(n):
    """n ADD instructions, each depending on the immediately-preceding
    one's result: R1=R0+R0, R2=R1+R0, R3=R2+R0, ... — maximum RAW
    hazard density."""
    instrs = [Instruction("ADD R1, R0, R0")]
    for i in range(2, n + 1):
        instrs.append(Instruction(f"ADD R{i}, R{i - 1}, R0"))
    return instrs


def make_independent_chain(n):
    """n ADD instructions with disjoint registers -- zero hazards."""
    return [Instruction(f"ADD R{2 * i}, R{2 * i + 1}, R{2 * i + 2}") for i in range(n)]


def make_load_use_chain(n):
    """n/2 LOAD-then-immediately-dependent-ADD pairs -- the classic
    load-use hazard, which even forwarding can't fully hide."""
    instrs = []
    for i in range(n // 2):
        instrs.append(Instruction(f"LOAD R{i}, 0(R0)"))
        instrs.append(Instruction(f"ADD R{100 + i}, R{i}, R0"))
    return instrs


def make_mixed_chain(n, hazard_every):
    """n instructions where every `hazard_every`-th one depends on the
    previous instruction's result, otherwise independent."""
    instrs = []
    for i in range(n):
        if i > 0 and i % hazard_every == 0:
            instrs.append(Instruction(f"ADD R{i}, R{i - 1}, R0"))
        else:
            instrs.append(Instruction(f"ADD R{i}, R{100 + i}, R{200 + i}"))
    return instrs


def report(label, instrs):
    with_fwd, cycles_fwd = simulate(instrs, forwarding=True)
    without_fwd, cycles_no_fwd = simulate(instrs, forwarding=False)
    s_fwd = stats(instrs, with_fwd, cycles_fwd)
    s_no_fwd = stats(instrs, without_fwd, cycles_no_fwd)
    reduction = 100.0 * (1 - s_fwd["stalls"] / s_no_fwd["stalls"]) if s_no_fwd["stalls"] > 0 else 0.0
    print(f"{label:<24}{s_no_fwd['stalls']:<16}{s_fwd['stalls']:<14}"
          f"{s_no_fwd['cpi']:<14.2f}{s_fwd['cpi']:<12.2f}{reduction:.1f}%")


def simulate_two_issue(instrs):
    ready = {}
    next_instruction = 0
    cycle = 1
    while next_instruction < len(instrs):
        issued = 0
        while issued < 2 and next_instruction < len(instrs):
            instr = instrs[next_instruction]
            if any(ready.get(src, 0) > cycle for src in instr.srcs):
                break
            latency = 2 if instr.opcode == "LOAD" else 1
            if instr.dest:
                ready[instr.dest] = cycle + latency
            next_instruction += 1
            issued += 1
        cycle += 1
    return cycle + 3


def report_superscalar(label, instrs):
    single = stats(instrs, *simulate(instrs, forwarding=True))
    dual_cycles = simulate_two_issue(instrs)
    ideal = len(instrs) + 4
    print(f"{label:<24}{single['cycles']:<16}{dual_cycles:<14}{single['cpi']:<14.2f}"
          f"{dual_cycles / len(instrs):.2f}")


def main():
    n = 50
    print("Benchmark: forwarding's effect on stalls, by hazard density\n")
    print(f"{'pattern':<24}{'stalls (no fwd)':<16}{'stalls (fwd)':<14}{'CPI (no fwd)':<14}{'CPI (fwd)':<12}reduction")
    report("independent (0%)", make_independent_chain(n))
    report("mixed (1-in-4)", make_mixed_chain(n, 4))
    report("mixed (1-in-2)", make_mixed_chain(n, 2))
    report("dependent chain (100%)", make_dependent_chain(n))
    report("load-use pairs", make_load_use_chain(n))

    if True:
        branch_trace = [Instruction("BEQ R1, R2, target"), Instruction("ADD R3, R4, R5")]
        branch_stages, branch_cycles = simulate(branch_trace, forwarding=True)
        branch_stalls = stats(branch_trace, branch_stages, branch_cycles)["stalls"]
        print(f"\nExperiment 1: stall-on-branch control hazard\n\n"
                    f"branch EX cycle: {branch_stages[0]['EX']}  younger IF cycle: {branch_stages[1]['IF']}  "
                    f"cycles: {branch_cycles}  stalls: {branch_stalls}\n")

    print("\nExperiment 2: two-issue in-order pipeline versus single issue\n")
    print(f"{'pattern':<24}{'single cycles':<16}{'dual cycles':<14}{'single CPI':<14}dual CPI")
    report_superscalar("independent (0%)", make_independent_chain(n))
    report_superscalar("dependent chain (100%)", make_dependent_chain(n))
    report_superscalar("load-use pairs", make_load_use_chain(n))

    print("\nExperiment 3: forwarding path ablation\n")
    print(f"{'pattern':<24}{'full stalls':<16}{'MEM/WB stalls':<16}regression")
    for label, instrs in [("dependent chain", make_dependent_chain(n)), ("load-use pairs", make_load_use_chain(n))]:
        full = stats(instrs, *simulate(instrs, forwarding=True))
        memwb = stats(instrs, *simulate(instrs, forwarding="memwb"))
        print(f"{label:<24}{full['stalls']:<16}{memwb['stalls']:<16}{memwb['stalls'] - full['stalls']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
