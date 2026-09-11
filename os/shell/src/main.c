#define _POSIX_C_SOURCE 200809L // isatty()/fileno()
#include "shell.h"

#include <unistd.h>

int main(void) {
    int interactive = isatty(fileno(stdin));
    return shell_loop(stdin, stdout, interactive);
}
