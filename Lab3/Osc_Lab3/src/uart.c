#include "common.h"
#include "uart.h"

/* =========================================================
 * 這裡先用 NS16550 相容 UART 的常見配置示範。
 *
 * 注意：
 * Orange Pi RV2 真正 UART base 可能依實際 device tree / firmware 設定而異。
 * 你之後若已知正確 base address，只要改這裡即可。
 *
 * 最小驗證版的目的是先把 kernel 架構與 allocator 流程搭起來。
 * =========================================================
 */
#define UART_BASE 0x10000000UL

#define UART_RHR 0x00  /* Receive Holding Register */
#define UART_THR 0x00  /* Transmit Holding Register */
#define UART_LSR 0x05  /* Line Status Register */

#define LSR_TX_IDLE 0x20

static inline void mmio_write8(u64 addr, u8 value)
{
    *(volatile u8 *)addr = value;
}

static inline u8 mmio_read8(u64 addr)
{
    return *(volatile u8 *)addr;
}

void uart_init(void)
{
    /* 最小版本先假設 firmware 已初始化 UART */
}

void uart_putc(char c)
{
    if (c == '\n') {
        uart_putc('\r');
    }

    while ((mmio_read8(UART_BASE + UART_LSR) & LSR_TX_IDLE) == 0) {
        /* busy wait */
    }

    mmio_write8(UART_BASE + UART_THR, (u8)c);
}

void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}