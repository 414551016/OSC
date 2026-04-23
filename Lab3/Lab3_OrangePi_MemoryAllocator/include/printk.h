#ifndef PRINTK_H
#define PRINTK_H

#include <stddef.h>
#include <stdint.h>

void printk(const char *fmt, ...);
int console_getc(void);
void console_putc(char c);
void console_puts(const char *s);

#endif