/*
 * Interrupt Handling Implementation
 * uOS(m) - User OS Mobile
 */

#include "interrupt.h"
#include "task.h"
#include "memory.h"
#include <stddef.h>

/* Fall back if config.h hasn't defined this */
#ifndef TIMER_TICK_MS
#define TIMER_TICK_MS 10
#endif

#define MAX_IRQS 256
interrupt_handler_t irq_handlers[MAX_IRQS];
irq_handler_t irq_handlers_irq[MAX_IRQS];
void *irq_priv[MAX_IRQS];

extern void uart_puts(const char *s);
extern void uart_putc(char c);
extern void uart_tx_interrupt(void);
extern void scheduler_tick(void);

static volatile uint32_t * const clint_mtime = (volatile uint32_t *)0x02004000;
static volatile uint32_t * const clint_mtimecmp = (volatile uint32_t *)0x0200400C;
static uint32_t timer_freq_hz;
static uint32_t timer_ticks_per_tick;

static uint64_t clint_read(void) {
    uint32_t lo, hi;
    do {
        hi = clint_mtime[1];
        lo = clint_mtime[0];
    } while (clint_mtime[1] != hi);
    return ((uint64_t)hi << 32) | lo;
}

static void clint_write_cmp(uint64_t when) {
    clint_mtimecmp[0] = (uint32_t)(when & 0xFFFFFFFFU);
    clint_mtimecmp[1] = (uint32_t)(when >> 32);
}

static void timer_schedule_next(void) {
    uint64_t now = clint_read();
    clint_write_cmp(now + timer_ticks_per_tick);
}

int
interrupt_register_handler_irq(uint32_t irq, irq_handler_t handler, void *priv)
{
    if (irq >= MAX_IRQS) return -1;
    irq_handlers_irq[irq] = handler;
    irq_priv[irq] = priv;
    return 0;
}

/* Initialize interrupt system */
int interrupt_init(void) {
    uart_puts("Interrupt system initializing...\n");

    for (int i = 0; i < MAX_IRQS; i++) {
        irq_handlers[i] = NULL;
        irq_handlers_irq[i] = NULL;
        irq_priv[i] = NULL;
    }

    /* Initialize PLIC (Platform Level Interrupt Controller) */
    volatile uint32_t *plic_priority = (volatile uint32_t *)0x0c000000;
    volatile uint32_t *plic_enable = (volatile uint32_t *)0x0c002000;
    volatile uint32_t *plic_threshold = (volatile uint32_t *)0x0c200000;

    plic_priority[10] = 1;
    plic_priority[1] = 1;

    plic_enable[0] = (1 << 10) | (1 << 1);

    *plic_threshold = 0;

    interrupt_register_handler(10, uart_tx_interrupt);

    uart_puts("Interrupt system ready\n");
    return 0;
}

/* Timer / clock interrupt */
void timer_init(uint32_t freq_hz) {
    timer_freq_hz = freq_hz;
    timer_ticks_per_tick = timer_freq_hz / (1000 / TIMER_TICK_MS);
    timer_schedule_next();
}

void timer_set_oneshot(uint64_t ticks) {
    uint64_t now = clint_read();
    clint_write_cmp(now + ticks);
}

void timer_ack(void) {
    timer_schedule_next();
}

/* Enable interrupts */
void interrupt_enable(void) {
    asm volatile("csrsi sstatus, 2");
}

/* Disable interrupts */
void interrupt_disable(void) {
    asm volatile("csrci sstatus, 2");
}

/* PLIC claim register for context 0 (used by entry.S trap handler) */
volatile const uint32_t plic_claim_base = 0x0c200004U;
