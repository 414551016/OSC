OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    . = 0x20000000;

    __kernel_start = .;

    .text : ALIGN(16) {
        *(.text.init)
        *(.text .text.*)
    }

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(16) {
        *(.data .data.*)
    }

    .bss : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(16);
    . += 0x4000;
    __stack_top = .;

    __kernel_end = .;
}