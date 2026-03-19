.section .text.boot
.globl _start

_start:
    # Save boot arguments from previous stage
    mv s0, a0              # hartid
    mv s1, a1              # dtb pointer

    # set stack
    la sp, boot_stack_top

    # pass hartid/dtb to C
    mv a0, s0
    mv a1, s1
    call boot_main

1:
    j 1b

.section .bss
.align 12
boot_stack:
    .space 4096
boot_stack_top: