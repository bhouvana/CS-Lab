// text bytecode -> Instruction array
#include "vm.h"

#include <stdlib.h>
#include <string.h>

const char *opcode_name(Opcode op) {
    switch (op) {
        case OP_PUSH: return "PUSH";
        case OP_POP: return "POP";
        case OP_ADD: return "ADD";
        case OP_SUB: return "SUB";
        case OP_MUL: return "MUL";
        case OP_DIV: return "DIV";
        case OP_LOAD: return "LOAD";
        case OP_STORE: return "STORE";
        case OP_JUMP: return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_PRINT: return "PRINT";
        case OP_HALT: return "HALT";
    }
    return "?";
}

static int mnemonic_to_opcode(const char *text, Opcode *out) {
    static const struct {
        const char *name;
        Opcode op;
    } table[] = {
        {"PUSH", OP_PUSH},          {"POP", OP_POP},   {"ADD", OP_ADD},
        {"SUB", OP_SUB},            {"MUL", OP_MUL},   {"DIV", OP_DIV},
        {"LOAD", OP_LOAD},          {"STORE", OP_STORE},
        {"JUMP", OP_JUMP},          {"JUMP_IF_FALSE", OP_JUMP_IF_FALSE},
        {"PRINT", OP_PRINT},        {"HALT", OP_HALT},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strcmp(text, table[i].name) == 0) {
            *out = table[i].op;
            return 1;
        }
    }
    return 0;
}

static int takes_operand(Opcode op) {
    return op == OP_PUSH || op == OP_LOAD || op == OP_STORE || op == OP_JUMP || op == OP_JUMP_IF_FALSE;
}

int assemble_file(const char *path, Program *program) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return -1;
    }

    Instruction *code = NULL;
    size_t count = 0, cap = 0;
    char line[256];
    int line_no = 0;
    int error = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        char mnemonic[64] = {0};
        long long operand = 0;
        int n = sscanf(line, "%63s %lld", mnemonic, &operand);
        if (n <= 0) continue; // blank (or comment-only) line

        Opcode op;
        if (!mnemonic_to_opcode(mnemonic, &op)) {
            fprintf(stderr, "line %d: unknown instruction '%s'\n", line_no, mnemonic);
            error = 1;
            break;
        }
        if (takes_operand(op) && n < 2) {
            fprintf(stderr, "line %d: %s requires an operand\n", line_no, mnemonic);
            error = 1;
            break;
        }

        if (count == cap) {
            cap = cap == 0 ? 64 : cap * 2;
            code = realloc(code, cap * sizeof(Instruction));
        }
        code[count].op = op;
        code[count].operand = (int64_t)operand;
        count++;
    }
    fclose(f);

    if (error) {
        free(code);
        return -1;
    }

    program->code = code;
    program->count = count;
    return 0;
}
