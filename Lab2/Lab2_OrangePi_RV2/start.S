.section .text.boot
.globl _start

_start:
    mv s0, a0              # hartid
    mv s1, a1              # dtb pointer

    la sp, boot_stack_top

    mv a0, s0
    mv a1, s1
    call boot_main

1:
    j 1b

.section .bss
.align 12
boot_stack:
    .space 16384
boot_stack_top: