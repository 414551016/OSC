#include "shell.h"
#include "sbi.h"
#include "string.h"
#include "uart.h"

#define CMD_BUF_SIZE 64

static void print_prompt(void)
{
    uart_puts("opi-rv2> ");
}

static void read_line(char *buf, int max_len)
{
    int i = 0;

    while (1) {
        char c = uart_getc();

        if (c == '\r' || c == '\n') {
            uart_putc('\n');
            break;
        }

        if (c == '\b' || c == 127) {
            if (i > 0) {
                i--;
                uart_puts("\b \b");
            }
            continue;
        }

        if (i < max_len - 1) {
            buf[i++] = c;
            uart_putc(c);
        }
    }

    buf[i] = '\0';
}

static void uart_put_uint(unsigned long n)
{
    char buf[32];
    int i = 0;

    if (n == 0) {
        uart_putc('0');
        return;
    }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

static void uart_put_hex(unsigned long n)
{
    static const char hex[] = "0123456789abcdef";
    int i;

    uart_puts("0x");

    for (i = (int)(sizeof(unsigned long) * 2) - 1; i >= 0; i--) {
        unsigned long v = (n >> (i * 4)) & 0xf;
        uart_putc(hex[v]);
    }
}

static void print_info(void)
{
    unsigned long spec = sbi_get_spec_version();
    unsigned long impl_id = sbi_get_impl_id();
    unsigned long impl_ver = sbi_get_impl_version();

    unsigned long major = (spec >> 24) & 0x7f;
    unsigned long minor = spec & 0xffffff;

    uart_puts("System information:\n");
    uart_puts("-------------------\n");
    uart_puts("  SBI spec version: ");
    uart_put_uint(major);
    uart_putc('.');
    uart_put_uint(minor);
    uart_putc('\n');

    uart_puts("  OpenSBI specification version: ");
    uart_put_hex(spec);
    uart_putc('\n');

    uart_puts("  SBI implementation ID: ");
    //uart_put_uint(impl_id);
    uart_put_hex(impl_id);
    uart_putc('\n');

    uart_puts("  SBI implementation version: ");
    uart_put_hex(impl_ver);
    uart_putc('\n');
}

static void execute_command(const char *cmd)
{
    if (strcmp(cmd, "help") == 0) {
        uart_puts("Available commands:\n");
        uart_puts("-------------------\n");
        uart_puts("  help   - show all commands.\n");
        uart_puts("  hello  - print Hello World.\n");
        uart_puts("  info   - print system info.\n");
    } else if (strcmp(cmd, "hello") == 0) {
        uart_puts("Hello World.\n");
    } else if (strcmp(cmd, "info") == 0) {
        print_info();
    } else if (strlen(cmd) == 0) {
        /* empty line */
    } else {
        uart_puts("Unknown command: ");
        uart_puts(cmd);
        uart_putc('\n');
        uart_puts("  Use 'help' to list commands.\n");
    }
}

void shell_run(void)
{
    char buf[CMD_BUF_SIZE];

    uart_puts("Simple shell started.\n");
    uart_puts("Type 'help' to list commands.\n");

    while (1) {
        print_prompt();
        read_line(buf, CMD_BUF_SIZE);
        execute_command(buf);
    }
}