// CLI: vm <program.bytecode> [--trace]
#include "vm.h"

#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <program.bytecode> [--trace]\n", argv[0]);
        return 2;
    }
    int trace = (argc >= 3 && strcmp(argv[2], "--trace") == 0);

    Program program;
    if (assemble_file(argv[1], &program) != 0) return 1;

    VM vm;
    vm_init(&vm, &program);
    int rc = vm_run(&vm, trace, stdout);

    free(program.code);
    return rc == 0 ? 0 : 1;
}
