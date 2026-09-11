/* A complete program with NO libc and NO C at all: _start is the raw
 * ELF entry point the kernel jumps to directly, and every observable
 * effect (printing, exiting) happens via a bare `syscall` instruction.
 * Build with -nostdlib (see Makefile) -- linking libc at all would
 * defeat the point.
 *
 *   user program -> register arguments -> syscall -> kernel -> return value
 */
    .section .rodata
msg:
    .ascii "Hello from a raw Linux syscall -- no libc involved.\n"
    msg_len = . - msg

    .text
    .global _start
_start:
    mov     $1, %rax          /* sys_write */
    mov     $1, %rdi          /* fd = STDOUT_FILENO */
    lea     msg(%rip), %rsi
    mov     $msg_len, %rdx
    syscall                   /* rax now holds bytes written (or a negative errno) */

    mov     $60, %rax         /* sys_exit */
    xor     %rdi, %rdi        /* status = 0 */
    syscall
