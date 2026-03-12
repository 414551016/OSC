ENTRY(_start)

SECTIONS
{
    . = 0x80200000;

    .text : ALIGN(8) {
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(8) {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(8) {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(8) {
        _bss_start = .;
        *(.sbss)
        *(.sbss.*)
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    }

    . = ALIGN(16);
    . = . + 0x4000;
    _stack_top = .;
}