/* Linux x86-64 syscall calling convention (distinct from the C ABI):
 *   syscall number -> RAX
 *   arguments      -> RDI, RSI, RDX, R10, R8, R9  (note: R10, not RCX --
 *                     the SYSCALL instruction itself clobbers RCX and
 *                     R11, so the kernel ABI moves the 4th argument to
 *                     R10 to avoid the collision)
 *   return value   -> RAX (a raw kernel result: success is >= 0, and a
 *                     failure is a NEGATED errno, e.g. -9 for EBADF --
 *                     this is what "no libc" really means: libc's
 *                     write() is the thing that turns a negative
 *                     return into "return -1, set the errno global").
 *
 * read, write, and exit all happen to take their first 3 (or fewer)
 * arguments in the same registers the C calling convention already
 * puts them in (RDI, RSI, RDX), so these wrappers barely have to move
 * anything -- they just load the syscall number and trap.
 */
    .text

/* long my_read(int fd, void *buf, unsigned long count) */
    .global my_read
my_read:
    mov     $0, %rax        /* sys_read */
    syscall
    ret

/* long my_write(int fd, const void *buf, unsigned long count) */
    .global my_write
my_write:
    mov     $1, %rax        /* sys_write */
    syscall
    ret

/* void my_exit(int status) -- never returns */
    .global my_exit
my_exit:
    mov     $60, %rax        /* sys_exit */
    syscall
    /* unreachable */

/* long my_getpid(void) */
    .global my_getpid
my_getpid:
    mov     $39, %rax        /* sys_getpid */
    syscall
    ret
