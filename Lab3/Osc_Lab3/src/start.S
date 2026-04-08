    .section .text.boot
    .globl _start
    .globl __bss_start
    .globl __bss_end
    .globl kernel_main

/* =========================================================
 * 最小啟動程式：
 * 1. 設定 stack
 * 2. 清空 BSS
 * 3. 跳到 C 的 kernel_main
 * =========================================================
 */
_start:
    /* 設定 stack pointer 到 boot_stack_top */
    la sp, boot_stack_top

    /* 清除 BSS 區段 */
    la t0, __bss_start
    la t1, __bss_end

1:
    bgeu t0, t1, 2f
    sd zero, 0(t0)
    addi t0, t0, 8
    j 1b

2:
    /* 進入 C 主程式 */
    call kernel_main

3:
    /* 若 kernel_main 返回，停在這裡 */
    wfi
    j 3b

    .section .bss
    .align 12
boot_stack:
    .space 16384
boot_stack_top: