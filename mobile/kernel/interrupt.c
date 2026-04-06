/*
 * Interrupt Handling Implementation
 * uOS(m) - User OS Mobile
 */

#include "interrupt.h"
#include <stddef.h>

#define MAX_IRQS 256
interrupt_handler_t irq_handlers[MAX_IRQS];

extern void uart_puts(const char *s);
extern void uart_tx_interrupt(void);

/* Register an interrupt handler */
int interrupt_register_handler(uint32_t irq, interrupt_handler_t handler) {
    if (irq >= MAX_IRQS) return -1;
    irq_handlers[irq] = handler;
    return 0;
}

/* Initialize interrupt system */
int interrupt_init(void) {
    uart_puts("Interrupt system initializing...\n");
    
    for (int i = 0; i < MAX_IRQS; i++) {
        irq_handlers[i] = NULL;
    }
    
    /* Initialize PLIC (Platform Level Interrupt Controller) */
    /* For QEMU virt machine, PLIC base is 0x0c000000 */
    volatile uint32_t *plic_priority = (volatile uint32_t *)0x0c000000;
    volatile uint32_t *plic_enable = (volatile uint32_t *)0x0c002000;
    volatile uint32_t *plic_threshold = (volatile uint32_t *)0x0c200000;
    
    /* Set priorities for UART (irq 10) and VirtIO (irq 1) */
    plic_priority[10] = 1;  /* UART */
    plic_priority[1] = 1;   /* VirtIO */
    
    /* Enable interrupts */
    plic_enable[0] = (1 << 10) | (1 << 1);
    
    /* Set threshold to 0 */
    *plic_threshold = 0;
    
    /* Register UART interrupt handler */
    interrupt_register_handler(10, uart_tx_interrupt);
    
    uart_puts("Interrupt system ready\n");
    return 0;
}

/* Enable interrupts */
void interrupt_enable(void) {
    asm volatile("csrsi sstatus, 2");  /* Set SIE bit */
}

/* Disable interrupts */
void interrupt_disable(void) {
    asm volatile("csrci sstatus, 2");  /* Clear SIE bit */
}