/*
 * uart_serial.c — global uart_puts / uart_putc symbols
 * These are used by many kernel subsystems for debug output.
 * kernel.c has its own static copies; this file provides the
 * globally visible versions that the linker can resolve.
 */

#include <stdint.h>

#define UART_BASE    0x10000000UL
#define UART_TX      0x0
#define UART_LSR     0x5
#define UART_LSR_TX  0x20

static volatile unsigned char *const _uart = (volatile unsigned char *)UART_BASE;

void uart_putc(char c) {
    while ((_uart[UART_LSR] & UART_LSR_TX) == 0) { }
    _uart[UART_TX] = (unsigned char)c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

void interrupt_register_handler(unsigned int irq, void (*handler)(void)) {
    /* Stub: full implementation is in interrupt.c via irq_handlers[] */
    (void)irq; (void)handler;
}
