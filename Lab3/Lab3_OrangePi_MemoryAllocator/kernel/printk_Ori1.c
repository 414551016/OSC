#include "printk.h"
#include <stdarg.h>
#include <stdint.h>

#define UART_BASE 0xd4017000UL
#define UART_REG_SHIFT 2

#define UART_RBR 0x00
#define UART_THR 0x00
#define UART_LSR (0x05 << UART_REG_SHIFT)

#define UART_LSR_DR   0x01
#define UART_LSR_THRE 0x20

#define SBI_CONSOLE_PUTCHAR 1
#define SBI_CONSOLE_GETCHAR 2

static inline volatile uint32_t *uart_reg(unsigned long off) {
    return (volatile uint32_t *)(UART_BASE + off);
}

static inline uint32_t uart_read(unsigned long off) {
    return *uart_reg(off);
}

static inline void uart_write(unsigned long off, uint32_t val) {
    *uart_reg(off) = val;
}

static inline void uart_putchar(char c) {
    while ((uart_read(UART_LSR) & UART_LSR_THRE) == 0) {
    }
    uart_write(UART_THR, (uint32_t)(unsigned char)c);
}

/* SBI legacy console getchar: return -1 if no char */
static inline long sbi_console_getchar(void) {
    register long a7 asm("a7") = SBI_CONSOLE_GETCHAR;
    register long a0 asm("a0");
    asm volatile("ecall"
                 : "=r"(a0)
                 : "r"(a7)
                 : "memory");
    return a0;
}

/* optional: SBI legacy console putchar */
static inline void sbi_console_putchar(int ch) {
    register long a0 asm("a0") = ch;
    register long a7 asm("a7") = SBI_CONSOLE_PUTCHAR;
    asm volatile("ecall"
                 :
                 : "r"(a0), "r"(a7)
                 : "memory");
}

void console_putc(char c) {
    if (c == '\n')
        uart_putchar('\r');
    uart_putchar(c);
}

void console_puts(const char *s) {
    while (*s)
        console_putc(*s++);
}

int console_getc(void) {
    long ch;

    do {
        ch = sbi_console_getchar();
    } while (ch < 0);

    return (int)ch;
}

static void print_unsigned(unsigned long long value, unsigned int base, int upper) {
    char buf[32];
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (value == 0) {
        console_putc('0');
        return;
    }

    while (value) {
        buf[i++] = digits[value % base];
        value /= base;
    }

    while (i)
        console_putc(buf[--i]);
}

static void print_signed(long long value) {
    if (value < 0) {
        console_putc('-');
        print_unsigned((unsigned long long)(-value), 10, 0);
    } else {
        print_unsigned((unsigned long long)value, 10, 0);
    }
}

void printk(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            console_putc(*fmt++);
            continue;
        }

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
            uintptr_t v = (uintptr_t)va_arg(ap, void *);
            console_puts("0x");
            print_unsigned((unsigned long long)v, 16, 0);
            break;
        }
        case 'l':
            fmt++;
            if (*fmt == 'x') {
                print_unsigned((unsigned long long)va_arg(ap, unsigned long), 16, 0);
            } else if (*fmt == 'u') {
                print_unsigned((unsigned long long)va_arg(ap, unsigned long), 10, 0);
            } else if (*fmt == 'd' || *fmt == 'i') {
                print_signed((long long)va_arg(ap, long));
            } else {
                console_putc('%');
                console_putc('l');
                console_putc(*fmt);
            }
            break;
        default:
            console_putc('%');
            console_putc(*fmt);
            break;
        }

        fmt++;
    }

    va_end(ap);
}