// Асемблерні заглушки обробників переривань. Кожна заглушка доповнює стек кодом помилки
// (якщо процесор його не передає) і номером вектора, після чого переходить до isr_common.
#include <stdint.h>

asm(R"(
.section .text

.macro ISR_NOERR num
.global isr_stub_\num
isr_stub_\num:
    pushq $0
    pushq $\num
    jmp isr_common
.endm

.macro ISR_ERR num
.global isr_stub_\num
isr_stub_\num:
    pushq $\num
    jmp isr_common
.endm

.irp n, 0,1,2,3,4,5,6,7,9,15,16,18,19,20,22,23,24,25,26,27,28,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
    ISR_NOERR \n
.endr
.irp n, 8,10,11,12,13,14,17,21,29,30
    ISR_ERR \n
.endr

isr_common:
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    movq %rsp, %rdi
    cld
    call isr_dispatch
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    addq $16, %rsp
    iretq

.section .rodata
.balign 8
.global isr_stub_table
isr_stub_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47
    .quad isr_stub_\n
.endr
.section .text
)");
