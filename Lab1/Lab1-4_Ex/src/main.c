#include "shell.h"
#include "uart.h"

void main(void)
{
    uart_init();
    uart_puts("NYCU OSC RISC-V KERNEL ID=414551016\n");
    uart_puts("Lab1 Exercise4 - System Information\n");
    shell_run();
}