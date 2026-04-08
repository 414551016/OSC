#ifndef UART_H
#define UART_H

/* 初始化 UART */
void uart_init(void);

/* 輸出單一字元 */
void uart_putc(char c);

/* 輸出字串 */
void uart_puts(const char *s);

#endif