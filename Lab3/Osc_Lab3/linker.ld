/* =========================================================
 * Lab3 minimal linker script
 *
 * 說明：
 * 1. 這裡先把 kernel 放到一個固定虛擬/實體位置。
 * 2. 真正實際載入位址，會依你後面用的啟動方式而定。
 * 3. 這份模板的重點是讓 kernel 內部結構完整且可獨立理解。
 * =========================================================
 */

ENTRY(_start)

SECTIONS
{
    /* 先假設 kernel 從 0x80200000 開始執行 */
    . = 0x80200000;

    .text : ALIGN(4K) {
        *(.text.boot)
        *(.text .text.*)
    }

    .rodata : ALIGN(4K) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4K) {
        *(.data .data.*)
    }

    .bss : ALIGN(4K) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }

    . = ALIGN(4K);
    __kernel_end = .;
}