/*同時產生二種版本：
kernel.fit → OrangePi RV2 用
kernel.img → WSL/QEMU 用
kernel.elf → OrangePi 版 ELF
qemu.elf → QEMU 版 ELF
*/
#include "uart.h"
#include <stdint.h>

#ifdef QEMU_VIRT
#define UART_BASE       0x10000000UL
#define REG_SHIFT       0
#define REG_IO_WIDTH    1
#else
#define UART_BASE       0xD4017000UL
#define REG_SHIFT       2
#define REG_IO_WIDTH    4
#endif

#define UART_RBR 0
#define UART_THR 0
#define UART_LSR 5

#define UART_LSR_DR     0x01
#define UART_LSR_THRE   0x20

static inline uintptr_t reg(unsigned int r)
{
    return UART_BASE + ((uintptr_t)r << REG_SHIFT);
}

#if REG_IO_WIDTH == 1
static inline uint8_t mmio_read(uintptr_t addr)
{
    return *(volatile uint8_t *)addr;
}

static inline void mmio_write(uintptr_t addr, uint8_t val)
{
    *(volatile uint8_t *)addr = val;
}
#else
static inline uint32_t mmio_read(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void mmio_write(uintptr_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}
#endif

void uart_init(void)
{
    /* OpenSBI / firmware 已先初始化 UART */
}

void uart_putc(char c)
{
    if (c == '\n') {
        uart_putc('\r');
    }

    while (!(mmio_read(reg(UART_LSR)) & UART_LSR_THRE)) {
    }

    mmio_write(reg(UART_THR), (unsigned char)c);
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