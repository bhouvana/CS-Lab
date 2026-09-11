// fetch -> decode -> execute -> repeat
#include "vm.h"

void vm_init(VM *vm, const Program *program) {
    vm->sp = 0;
    for (int i = 0; i < MEMORY_SLOTS; i++) vm->memory[i] = 0;
    vm->program = program;
    vm->pc = 0;
}

static int do_push(VM *vm, int64_t value) {
    if (vm->sp >= STACK_MAX) {
        fprintf(stderr, "stack overflow at pc=%zu\n", vm->pc);
        return -1;
    }
    vm->stack[vm->sp++] = value;
    return 0;
}

static int do_pop(VM *vm, int64_t *out) {
    if (vm->sp <= 0) {
        fprintf(stderr, "stack underflow at pc=%zu\n", vm->pc);
        return -1;
    }
    *out = vm->stack[--vm->sp];
    return 0;
}

static int valid_slot(VM *vm, int64_t slot) {
    if (slot < 0 || slot >= MEMORY_SLOTS) {
        fprintf(stderr, "invalid memory slot %lld at pc=%zu\n", (long long)slot, vm->pc);
        return 0;
    }
    return 1;
}

static int valid_target(VM *vm, int64_t target) {
    if (target < 0 || (size_t)target >= vm->program->count) {
        fprintf(stderr, "invalid jump target %lld at pc=%zu\n", (long long)target, vm->pc);
        return 0;
    }
    return 1;
}

static void print_trace_line(FILE *out, Instruction instr) {
    switch (instr.op) {
        case OP_PUSH:
        case OP_LOAD:
        case OP_STORE:
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
            fprintf(out, "%s %lld\n", opcode_name(instr.op), (long long)instr.operand);
            break;
        default:
            fprintf(out, "%s\n", opcode_name(instr.op));
    }
}

static void print_stack(FILE *out, const VM *vm) {
    fprintf(out, "STACK:\n[");
    for (int i = 0; i < vm->sp; i++) {
        if (i) fprintf(out, ", ");
        fprintf(out, "%lld", (long long)vm->stack[i]);
    }
    fprintf(out, "]\n\n");
}

int vm_run(VM *vm, int trace, FILE *out) {
    const Program *p = vm->program;

    for (;;) {
        if (vm->pc >= p->count) {
            fprintf(stderr, "ran off the end of the program without HALT\n");
            return -1;
        }
        Instruction instr = p->code[vm->pc];
        if (trace) print_trace_line(out, instr);

        size_t next_pc = vm->pc + 1;
        // Initialized only to silence a false-positive -Wmaybe-uninitialized:
        // every read below is reached only after the do_pop() that sets it
        // has already returned successfully.
        int64_t a = 0, b = 0;

        switch (instr.op) {
            case OP_PUSH:
                if (do_push(vm, instr.operand) != 0) return -1;
                break;
            case OP_POP:
                if (do_pop(vm, &a) != 0) return -1;
                break;
            case OP_ADD:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (do_push(vm, a + b) != 0) return -1;
                break;
            case OP_SUB:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (do_push(vm, a - b) != 0) return -1;
                break;
            case OP_MUL:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (do_push(vm, a * b) != 0) return -1;
                break;
            case OP_DIV:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (b == 0) {
                    fprintf(stderr, "division by zero at pc=%zu\n", vm->pc);
                    return -1;
                }
                if (do_push(vm, a / b) != 0) return -1;
                break;
            case OP_MOD:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (b == 0) {
                    fprintf(stderr, "modulo by zero at pc=%zu\n", vm->pc);
                    return -1;
                }
                if (do_push(vm, a % b) != 0) return -1;
                break;
            case OP_LT:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (do_push(vm, a < b) != 0) return -1;
                break;
            case OP_LE:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (do_push(vm, a <= b) != 0) return -1;
                break;
            case OP_GT:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (do_push(vm, a > b) != 0) return -1;
                break;
            case OP_GE:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (do_push(vm, a >= b) != 0) return -1;
                break;
            case OP_EQ:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (do_push(vm, a == b) != 0) return -1;
                break;
            case OP_NE:
                if (do_pop(vm, &b) != 0 || do_pop(vm, &a) != 0) return -1;
                if (do_push(vm, a != b) != 0) return -1;
                break;
            case OP_LOAD:
                if (!valid_slot(vm, instr.operand)) return -1;
                if (do_push(vm, vm->memory[instr.operand]) != 0) return -1;
                break;
            case OP_STORE:
                if (!valid_slot(vm, instr.operand)) return -1;
                if (do_pop(vm, &a) != 0) return -1;
                vm->memory[instr.operand] = a;
                break;
            case OP_JUMP:
                if (!valid_target(vm, instr.operand)) return -1;
                next_pc = (size_t)instr.operand;
                break;
            case OP_JUMP_IF_FALSE:
                if (do_pop(vm, &a) != 0) return -1;
                if (a == 0) {
                    if (!valid_target(vm, instr.operand)) return -1;
                    next_pc = (size_t)instr.operand;
                }
                break;
            case OP_PRINT:
                if (do_pop(vm, &a) != 0) return -1;
                fprintf(out, "%lld\n", (long long)a);
                break;
            case OP_HALT:
                if (trace) print_stack(out, vm);
                return 0;
            default:
                // Unreachable via the text assembler (it only ever emits
                // named mnemonics), but Program/Instruction are a public,
                // constructible API -- nothing stops a caller building one
                // directly with an out-of-range Opcode value. Without this,
                // the switch matched nothing and silently fell through to
                // "advance pc, keep going", which is exactly the "quietly
                // execute malformed bytecode" behavior CS-LAB.md forbids.
                fprintf(stderr, "invalid opcode %d at pc=%zu\n", (int)instr.op, vm->pc);
                return -1;
        }

        if (trace && instr.op != OP_HALT) print_stack(out, vm);
        vm->pc = next_pc;
    }
}
