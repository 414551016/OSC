#include "printk.h"
#include <stdarg.h>
#include <stdint.h>

/* UART 實體基底位址 */
#define UART_BASE 0xd4017000UL

/* UART register spacing:
 * 此平台 UART register 每個暫存器間隔 4 bytes
 * 因此 register offset 需要左移 2
 */
#define UART_REG_SHIFT 2

/* 16550 / DW-APB UART 常用暫存器 */
#define UART_RBR 0x00                      /* Receive Buffer Register */
#define UART_THR 0x00                      /* Transmit Holding Register */
#define UART_LSR (0x05 << UART_REG_SHIFT)  /* Line Status Register */

/* LSR bits */
#define UART_LSR_DR   0x01  /* Data Ready：接收緩衝區有資料 */
#define UART_LSR_THRE 0x20  /* THR Empty：可寫入下一個字元 */

/* SBI legacy console function IDs */
#define SBI_CONSOLE_PUTCHAR 1
#define SBI_CONSOLE_GETCHAR 2

/* 取得某個 UART register 的 MMIO 位址 */
static inline volatile uint32_t *uart_reg(unsigned long off) {
    return (volatile uint32_t *)(UART_BASE + off);
}

/* 從 UART register 讀 32-bit 值 */
static inline uint32_t uart_read(unsigned long off) {
    return *uart_reg(off);
}

/* 寫 32-bit 值到 UART register */
static inline void uart_write(unsigned long off, uint32_t val) {
    *uart_reg(off) = val;
}

/* 輸出單一字元到 UART
 * 流程：
 * 1. 等待 THR 空
 * 2. 將字元寫入 THR
 */
static inline void uart_putchar(char c) {
    while ((uart_read(UART_LSR) & UART_LSR_THRE) == 0) {
    }
    uart_write(UART_THR, (uint32_t)(unsigned char)c);
}

/* SBI legacy console getchar
 * 若目前沒有字元可讀，回傳 -1
 * 透過 ecall 呼叫 OpenSBI / firmware 提供的 console input
 */
static inline long sbi_console_getchar(void) {
    register long a7 asm("a7") = SBI_CONSOLE_GETCHAR;
    register long a0 asm("a0");

    asm volatile("ecall"
                 : "=r"(a0)
                 : "r"(a7)
                 : "memory");
    return a0;
}

/* SBI legacy console putchar
 * 這裡目前沒有在主流程使用，保留作為備用介面
 */
static inline void sbi_console_putchar(int ch) {
    register long a0 asm("a0") = ch;
    register long a7 asm("a7") = SBI_CONSOLE_PUTCHAR;

    asm volatile("ecall"
                 :
                 : "r"(a0), "r"(a7)
                 : "memory");
}

/* 主控台輸出單一字元
 * 若遇到 '\n'，先補 '\r'，讓 serial terminal 正確換行
 * 最後實際仍由 UART MMIO 輸出
 */
void console_putc(char c) {
    if (c == '\n')
        uart_putchar('\r');
    uart_putchar(c);
}

/* 主控台輸出字串 */
void console_puts(const char *s) {
    while (*s)
        console_putc(*s++);
}

/* 主控台輸入單一字元
 * 這裡不直接從 UART RX register 讀，
 * 而是透過 SBI legacy console getchar 取得輸入
 * 若沒有字元，持續 busy-wait
 */
int console_getc(void) {
    long ch;

    do {
        ch = sbi_console_getchar();
    } while (ch < 0);

    return (int)ch;
}

/* 輸出無號數字
 * 支援任意進位（目前用於 10 進位 / 16 進位）
 * upper=1 時使用大寫十六進位字元
 */
static void print_unsigned(unsigned long long value, unsigned int base, int upper) {
    char buf[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    /* 特判 0 */
    if (value == 0) {
        console_putc('0');
        return;
    }

    /* 先反向存進 buf */
    while (value) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    /* 再反向輸出 */
    while (i)
        console_putc(buf[--i]);
}

/* 輸出有號整數 */
static void print_signed(long long value) {
    if (value < 0) {
        console_putc('-');
        print_unsigned((unsigned long long)(-value), 10, 0);
    } else {
        print_unsigned((unsigned long long)value, 10, 0);
    }
}

/* 簡化版 printk
 * 支援的格式包含：
 *   %%  %c  %s
 *   %d  %i  %u
 *   %x  %X  %p
 *   %lx %lu %ld / %li
 *
 * 這是 kernel/bare-metal 環境下的簡易 printf 實作
 */
void printk(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        /* 一般字元直接輸出 */
        if (*fmt != '%') {
            console_putc(*fmt++);
            continue;
        }

        /* 遇到 %，解析格式 */
        fmt++;
        if (*fmt == '\0')
            break;

        switch (*fmt) {
        case '%':
            console_putc('%');
            break;

        case 'c':
            console_putc((char)va_arg(ap, int));
            break;

        case 's': {
            const char *s = va_arg(ap, const char *);
            console_puts(s ? s : "(null)");
            break;
        }

        case 'd':
        case 'i':
            print_signed((long long)va_arg(ap, int));
            break;

        case 'u':
            print_unsigned((unsigned long long)va_arg(ap, unsigned int), 10, 0);
            break;

        case 'x':
            print_unsigned((unsigned long long)va_arg(ap, unsigned int), 16, 0);
            break;

        case 'X':
            print_unsigned((unsigned long long)va_arg(ap, unsigned int), 16, 1);
            break;

        case 'p': {
            /* 指標輸出：固定補 0x 前綴 */
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            console_puts("0x");
            print_unsigned((unsigned long long)v, 16, 0);
            break;
        }

        case 'l':
            /* 支援 long 型別的部分格式 */
            fmt++;
            if (*fmt == 'x') {
                print_unsigned((unsigned long long)va_arg(ap, unsigned long), 16, 0);
            } else if (*fmt == 'u') {
                print_unsigned((unsigned long long)va_arg(ap, unsigned long), 10, 0);
            } else if (*fmt == 'd' || *fmt == 'i') {
                print_signed((long long)va_arg(ap, long));
            } else {
                /* 不支援的格式，原樣印出 */
                console_putc('%');
                console_putc('l');
                console_putc(*fmt);
            }
            break;

        default:
            /* 不支援的格式，原樣印出 */
            console_putc('%');
            console_putc(*fmt);
            break;
        }

        fmt++;
    }

    va_end(ap);
}