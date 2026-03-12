#include "uart.h"
#include <stdint.h>

#define UART_BASE 0x10000000UL

#define UART_RBR 0
#define UART_THR 0
#define UART_LSR 5

#define UART_LSR_DR   0x01
#define UART_LSR_THRE 0x20

static inline uintptr_t reg(unsigned int r)
{
    return UART_BASE + (uintptr_t)r;
}

static inline uint8_t mmio_read8(uintptr_t addr)
{
    return *(volatile uint8_t *)addr;
}

static inline void mmio_write8(uintptr_t addr, uint8_t val)
{
    *(volatile uint8_t *)addr = val;
}

void uart_init(void)
{
    /* QEMU + OpenSBI 已先初始化 UART */
}

void uart_putc(char c)
{
    if (c == '\n') {
        uart_putc('\r');
    }

    while (!(mmio_read8(reg(UART_LSR)) & UART_LSR_THRE)) {
    }

    mmio_write8(reg(UART_THR), (uint8_t)c);
}

char uart_getc(void)
{
    while (!(mmio_read8(reg(UART_LSR)) & UART_LSR_DR)) {
    }

    return (char)(mmio_read8(reg(UART_RBR)) & 0xff);
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}