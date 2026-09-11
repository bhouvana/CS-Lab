/* x86-64 System V calling convention, by hand.
 *
 * Integer/pointer arguments 1-6: RDI, RSI, RDX, RCX, R8, R9.
 * Return value: RAX.
 * Caller-saved (a call may clobber these; caller must save if it
 * needs them after): RAX, RCX, RDX, RSI, RDI, R8-R11.
 * Callee-saved (a call must restore these before returning, if it
 * uses them): RBX, RBP, R12-R15, RSP.
 */
    .text

/* long add2(long a, long b) -- rdi, rsi -> rax */
    .global add2
add2:
    mov     %rdi, %rax
    add     %rsi, %rax
    ret

/* long add3(long a, long b, long c) -- rdi, rsi, rdx -> rax */
    .global add3
add3:
    mov     %rdi, %rax
    add     %rsi, %rax
    add     %rdx, %rax
    ret

/* long uses_callee_saved(long a)
 * Clobbers RBX (a callee-saved register), so it must save it on entry
 * and restore it before returning -- that save/restore IS the ABI
 * contract, not an optional nicety. Returns a*2. */
    .global uses_callee_saved
uses_callee_saved:
    push    %rbx            /* save caller's RBX */
    mov     %rdi, %rbx
    imul    $2, %rbx
    mov     %rbx, %rax
    pop     %rbx             /* restore caller's RBX before returning */
    ret

/* long check_rbx_preserved(void)
 * Sets RBX to a sentinel, calls uses_callee_saved(5) (which clobbers
 * and restores RBX internally), then checks RBX still holds the
 * sentinel. Returns 1 if the callee-saved contract held, 0 if not --
 * a runtime proof of the contract, not just a comment asserting it. */
    .global check_rbx_preserved
check_rbx_preserved:
    push    %rbx             /* save OUR caller's RBX; we're about to clobber it for the test */
    mov     $0x1234, %rbx
    mov     $5, %rdi
    call    uses_callee_saved
    mov     $1, %rax
    cmp     $0x1234, %rbx
    je      1f
    mov     $0, %rax
1:
    pop     %rbx
    ret

/* long sum_three_locals(long a, long b, long c)
 * Spills all three arguments into local stack slots, reloads them,
 * and calls add3 -- demonstrating local variables AND that RSP must
 * stay 16-byte aligned immediately before a CALL even with locals in
 * play. 32 bytes of locals keeps RSP 16-aligned after CALL's implicit
 * push of the 8-byte return address (16 - 8 = 8, plus 32 keeps the
 * arithmetic aligned -- see the README for the full accounting). */
    .global sum_three_locals
sum_three_locals:
    sub     $32, %rsp
    mov     %rdi, 0(%rsp)
    mov     %rsi, 8(%rsp)
    mov     %rdx, 16(%rsp)
    mov     0(%rsp), %rdi
    mov     8(%rsp), %rsi
    mov     16(%rsp), %rdx
    call    add3
    add     $32, %rsp
    ret
