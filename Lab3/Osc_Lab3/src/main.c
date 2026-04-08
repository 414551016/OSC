#include "common.h"
#include "uart.h"
#include "printk.h"

void kernel_main(void)
{
    uart_init();

    uart_puts("\n[LAB3] entered kernel_main\n");
    uart_puts("[LAB3] uart direct output works\n");

    while (1) {
        asm volatile("wfi");
    }
}