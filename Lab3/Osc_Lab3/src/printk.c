#include <stdarg.h>
#include "common.h"
#include "uart.h"
#include "printk.h"

/* 輸出無號整數為 16 進位 */
static void print_hex(u64 value)
{
    char buf[17];
    const char *hex = "0123456789abcdef";

    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[value & 0xf];
        value >>= 4;
    }
    buf[16] = '\0';

    uart_puts("0x");
    uart_puts(buf);
}

/* 輸出無號整數為 10 進位 */
static void print_dec(u64 value)
{
    char buf[32];
    int i = 0;

    if (value == 0) {
        uart_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

void printk(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            uart_putc(*fmt++);
            continue;
        }

        fmt++;

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            uart_puts(s ? s : "(null)");
            break;
        }
        case 'c': {
            int c = va_arg(ap, int);
            uart_putc((char)c);
            break;
        }
        case 'x': {
            u64 v = va_arg(ap, u64);
            print_hex(v);
            break;
        }
        case 'd': {
            u64 v = va_arg(ap, u64);
            print_dec(v);
            break;
        }
        case '%':
            uart_putc('%');
            break;
        default:
            uart_putc('%');
            uart_putc(*fmt);
            break;
        }

        fmt++;
    }

    va_end(ap);
}