#include "uart.h"
#include <stdint.h>

#define UART_BASE 0xD4017000UL
#define REG_SHIFT 2

#define UART_RBR 0
#define UART_THR 0
#define UART_LSR 5

#define UART_LSR_DR   0x01
#define UART_LSR_THRE 0x20

static inline uintptr_t reg(unsigned int r)
{
    return UART_BASE + ((uintptr_t)r << REG_SHIFT);
}

static inline uint32_t mmio_read(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void mmio_write(uintptr_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

void uart_init(void)
{
    /* firmware 已先初始化 UART */
}

void uart_putc(char c)
{
    if (c == '\n') {
        uart_putc('\r');
    }

    while (!(mmio_read(reg(UART_LSR)) & UART_LSR_THRE)) {
    }

    mmio_write(reg(UART_THR), (uint32_t)(unsigned char)c);
}

char uart_getc(void)
{
    while (!(mmio_read(reg(UART_LSR)) & UART_LSR_DR)) {
    }

    return (char)(mmio_read(reg(UART_RBR)) & 0xff);
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}