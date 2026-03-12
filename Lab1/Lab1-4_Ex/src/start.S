.section .text
.global _start
.extern main
.extern clear_bss
.extern _stack_top

_start:
    la sp, _stack_top
    call clear_bss
    call main

1:
    j 1b