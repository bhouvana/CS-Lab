// text bytecode -> Instruction array
#include "vm.h"

#include <ctype.h>
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
        case OP_MOD: return "MOD";
        case OP_LT: return "LT";
        case OP_LE: return "LE";
        case OP_GT: return "GT";
        case OP_GE: return "GE";
        case OP_EQ: return "EQ";
        case OP_NE: return "NE";
        case OP_LOAD: return "LOAD";
        case OP_STORE: return "STORE";
        case OP_JUMP: return "JUMP";
        case OP_JUMP_IF_FALSE: return "JUMP_IF_FALSE";
        case OP_CALL: return "CALL";
        case OP_RET: return "RET";
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
        {"MOD", OP_MOD},            {"LT", OP_LT},     {"LE", OP_LE},
        {"GT", OP_GT},              {"GE", OP_GE},     {"EQ", OP_EQ},
        {"NE", OP_NE},
        {"LOAD", OP_LOAD},          {"STORE", OP_STORE},
        {"JUMP", OP_JUMP},          {"JUMP_IF_FALSE", OP_JUMP_IF_FALSE},
        {"CALL", OP_CALL},          {"RET", OP_RET},
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
    return op == OP_PUSH || op == OP_LOAD || op == OP_STORE || op == OP_JUMP || op == OP_JUMP_IF_FALSE ||
           op == OP_CALL;
}

// A local, portable dup: avoids relying on POSIX/BSD strdup, which
// needs a feature-test macro on glibc under -std=c11 and isn't
// guaranteed present at all on every C11 toolchain.
static char *dup_str(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

typedef struct {
    char name[64];
    size_t index; // the instruction index right after this label
} Label;

// `text` is already trimmed. A label definition is a bare identifier
// (letters/digits/underscore, not starting with a digit) followed by
// ':' and nothing else on the line -- e.g. "loop:". Writes the name
// (without the colon) into `name_out` (>= 64 bytes) and returns 1, or
// returns 0 if `text` isn't a label definition at all.
static int parse_label_def(const char *text, char *name_out) {
    size_t len = strlen(text);
    if (len < 2 || text[len - 1] != ':') return 0;
    size_t name_len = len - 1;
    if (name_len >= 64 || isdigit((unsigned char)text[0])) return 0;
    for (size_t i = 0; i < name_len; i++) {
        if (!isalnum((unsigned char)text[i]) && text[i] != '_') return 0;
    }
    memcpy(name_out, text, name_len);
    name_out[name_len] = '\0';
    return 1;
}

static int find_label(const Label *labels, size_t count, const char *name, size_t *out_index) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            *out_index = labels[i].index;
            return 1;
        }
    }
    return 0;
}

static void trim(char *s) {
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
    if (start != s) memmove(s, start, (size_t)(end - start) + 1);
}

int assemble_file(const char *path, Program *program) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return -1;
    }

    // Pass 1: strip comments and whitespace, split label definitions
    // ("loop:") from real instruction lines, and record where each
    // label points -- the instruction index right after it, since a
    // label defines a position, not an instruction of its own.
    char **inst_lines = NULL;
    int *inst_line_no = NULL;
    size_t inst_count = 0, inst_cap = 0;
    Label *labels = NULL;
    size_t label_count = 0, label_cap = 0;

    char raw[256];
    int line_no = 0;
    while (fgets(raw, sizeof(raw), f)) {
        line_no++;
        char *hash = strchr(raw, '#');
        if (hash) *hash = '\0';
        trim(raw);
        if (raw[0] == '\0') continue; // blank or comment-only line

        char label_name[64];
        if (parse_label_def(raw, label_name)) {
            if (label_count == label_cap) {
                label_cap = label_cap == 0 ? 16 : label_cap * 2;
                labels = realloc(labels, label_cap * sizeof(Label));
            }
            strcpy(labels[label_count].name, label_name);
            labels[label_count].index = inst_count;
            label_count++;
            continue;
        }

        if (inst_count == inst_cap) {
            inst_cap = inst_cap == 0 ? 64 : inst_cap * 2;
            inst_lines = realloc(inst_lines, inst_cap * sizeof(char *));
            inst_line_no = realloc(inst_line_no, inst_cap * sizeof(int));
        }
        inst_lines[inst_count] = dup_str(raw);
        inst_line_no[inst_count] = line_no;
        inst_count++;
    }
    fclose(f);

    // Pass 2: parse each real instruction line now that every label's
    // target index is known, whichever line (before or after the jump
    // that references it) the label itself was defined on.
    Instruction *code = malloc((inst_count > 0 ? inst_count : 1) * sizeof(Instruction));
    int error = 0;
    for (size_t i = 0; i < inst_count && !error; i++) {
        char mnemonic[64] = {0};
        char operand_text[64] = {0};
        int n = sscanf(inst_lines[i], "%63s %63s", mnemonic, operand_text);

        Opcode op;
        if (!mnemonic_to_opcode(mnemonic, &op)) {
            fprintf(stderr, "line %d: unknown instruction '%s'\n", inst_line_no[i], mnemonic);
            error = 1;
            break;
        }

        int64_t operand = 0;
        if (takes_operand(op)) {
            if (n < 2) {
                fprintf(stderr, "line %d: %s requires an operand\n", inst_line_no[i], mnemonic);
                error = 1;
                break;
            }
            char *endptr;
            long long parsed = strtoll(operand_text, &endptr, 10);
            if (endptr != operand_text && *endptr == '\0') {
                operand = (int64_t)parsed; // a plain numeric operand, same as before labels existed
            } else if (op == OP_JUMP || op == OP_JUMP_IF_FALSE || op == OP_CALL) {
                size_t target;
                if (!find_label(labels, label_count, operand_text, &target)) {
                    fprintf(stderr, "line %d: undefined label '%s'\n", inst_line_no[i], operand_text);
                    error = 1;
                    break;
                }
                operand = (int64_t)target;
            } else {
                fprintf(stderr, "line %d: '%s' is not a valid operand for %s\n", inst_line_no[i], operand_text,
                        mnemonic);
                error = 1;
                break;
            }
        }

        code[i].op = op;
        code[i].operand = operand;
    }

    for (size_t i = 0; i < inst_count; i++) free(inst_lines[i]);
    free(inst_lines);
    free(inst_line_no);
    free(labels);

    if (error) {
        free(code);
        return -1;
    }

    program->code = code;
    program->count = inst_count;
    return 0;
}
