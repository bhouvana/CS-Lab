#!/usr/bin/env python3
"""5-stage pipeline simulator: IF ID EX MEM WB.

    instructions (ADD/SUB/MUL/LOAD)
            |
            v
        pipeline           IF ID EX MEM WB, one instruction issued per cycle
            |
            v
    cycle table + RAW-hazard stalls (with/without forwarding) + CPI

Only RAW (read-after-write) hazards are actually simulated. WAR and WAW
hazards are discussed in the README but cannot occur in this design:
a single-issue, in-order pipeline always reads a register (in ID) and
writes it (in WB) in strict program order, so a later instruction can
never write before an earlier one reads (WAR) or write before an
earlier one writes (WAW) — those hazards only become possible with
out-of-order execution, which is out of scope here (see CS-LAB.md §48
on the relationship to the separate Aegis-X86 project).
"""
import re
import sys

STAGES = ["IF", "ID", "EX", "MEM", "WB"]


class Instruction:
    """One decoded instruction: opcode, destination register (or None),
    and the list of source registers it reads."""

    def __init__(self, text):
        self.text = text.strip()
        parts = re.split(r"[,\s]+", self.text)
        self.opcode = parts[0].upper()
        operands = parts[1:]

        if self.opcode == "LOAD":
            # LOAD Rd, offset(Rs)  -- reads only the base register Rs.
            self.dest = operands[0]
            match = re.match(r"-?\d+\((R\d+)\)", operands[1])
            self.srcs = [match.group(1) if match else operands[1]]
        else:
            # ADD/SUB/MUL Rd, Rs1, Rs2
            self.dest = operands[0]
            self.srcs = operands[1:]

    def __repr__(self):
        return self.text


def parse_program(lines):
    return [Instruction(line) for line in lines if line.strip() and not line.strip().startswith("#")]


def _ready_cycle(instructions, i, ex, mem, wb, forwarding):
    """Earliest cycle instruction i's EX stage may begin, given the
    most recent prior writer of each of its source registers."""
    earliest = 1
    for src in instructions[i].srcs:
        producer = None
        for j in range(i - 1, -1, -1):
            if instructions[j].dest == src:
                producer = j
                break
        if producer is None:
            continue
        if not forwarding:
            ready = wb[producer] + 1
        elif instructions[producer].opcode == "LOAD":
            # Load-use hazard: even with forwarding, a LOAD's value
            # isn't available until after its MEM stage, not its EX
            # stage like every other instruction.
            ready = mem[producer] + 1
        else:
            ready = ex[producer] + 1
        earliest = max(earliest, ready)
    return earliest


def simulate(instructions, forwarding):
    """Returns (stage_start, total_cycles) where stage_start[i] is a
    dict {stage_name: first_cycle_occupied} for instruction i. Only IF
    and ID can span multiple cycles (a stall); EX/MEM/WB always last
    exactly one cycle once entered — backpressure from a stall always
    propagates backward through the pipeline, never forward."""
    n = len(instructions)
    IF, ID, EX, MEM, WB = [0] * n, [0] * n, [0] * n, [0] * n, [0] * n

    for i in range(n):
        IF[i] = 1 if i == 0 else ID[i - 1]  # IF frees up once i-1 enters ID
        ID[i] = max(IF[i] + 1, (EX[i - 1] if i > 0 else 1))  # ID frees up once i-1 enters EX
        hazard_ready = _ready_cycle(instructions, i, EX, MEM, WB, forwarding)
        EX[i] = max(ID[i] + 1, (MEM[i - 1] if i > 0 else 1), hazard_ready)
        MEM[i] = max(EX[i] + 1, (WB[i - 1] if i > 0 else 1))
        WB[i] = max(MEM[i] + 1, (WB[i - 1] + 1 if i > 0 else 1))

    stage_start = [{"IF": IF[i], "ID": ID[i], "EX": EX[i], "MEM": MEM[i], "WB": WB[i]} for i in range(n)]
    total_cycles = WB[-1] if n > 0 else 0
    return stage_start, total_cycles


def stats(instructions, stage_start, total_cycles):
    n = len(instructions)
    ideal_cycles = n + (len(STAGES) - 1) if n > 0 else 0  # zero-hazard lower bound
    stalls = total_cycles - ideal_cycles
    cpi = total_cycles / n if n > 0 else 0.0
    return {"instructions": n, "cycles": total_cycles, "stalls": stalls, "cpi": cpi}


# IF and ID both start with "I", so stage identity needs an explicit
# letter map rather than stage_name[0] (directive's own example uses
# exactly these letters: F D E M W).
STAGE_LETTER = {"IF": "F", "ID": "D", "EX": "E", "MEM": "M", "WB": "W"}


def format_cycle_table(instructions, stage_start, total_cycles):
    header = "Cycle  " + " ".join(f"{c:>2}" for c in range(1, total_cycles + 1))
    lines = [header]
    for i, instr in enumerate(instructions):
        s = stage_start[i]
        # Stage occupancy ranges: [start_of_stage, start_of_next_stage).
        ranges = {
            "IF": (s["IF"], s["ID"]),
            "ID": (s["ID"], s["EX"]),
            "EX": (s["EX"], s["MEM"]),
            "MEM": (s["MEM"], s["WB"]),
            "WB": (s["WB"], s["WB"] + 1),
        }
        cells = []
        for cycle in range(1, total_cycles + 1):
            letter = ""
            for stage, (start, end) in ranges.items():
                if start <= cycle < end:
                    letter = STAGE_LETTER[stage]
                    break
            cells.append(f"{letter:>2}")
        lines.append(f"{instr.opcode:<7}" + " ".join(cells))
    return "\n".join(lines)


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <program-file> [--no-forwarding]", file=sys.stderr)
        return 2
    with open(sys.argv[1]) as f:
        instructions = parse_program(f.readlines())

    for forwarding, label in [(True, "WITH forwarding"), (False, "WITHOUT forwarding")]:
        stage_start, total_cycles = simulate(instructions, forwarding)
        s = stats(instructions, stage_start, total_cycles)
        print(f"=== {label} ===\n")
        print(format_cycle_table(instructions, stage_start, total_cycles))
        print(f"\ninstructions: {s['instructions']}  cycles: {s['cycles']}  "
              f"stalls: {s['stalls']}  CPI: {s['cpi']:.2f}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
