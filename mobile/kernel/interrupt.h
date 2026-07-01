/*
 * Interrupt Handling Framework
 * uOS(m) - User OS Mobile
 */

#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <stdint.h>

/* Interrupt handler function type */
typedef void (*interrupt_handler_t)(void);
typedef void (*irq_handler_t)(uint32_t irq, void *priv);

/* Register an interrupt handler */
int interrupt_register_handler(uint32_t irq, interrupt_handler_t handler);
int interrupt_register_handler_irq(uint32_t irq, irq_handler_t handler, void *priv);

/* Initialize interrupt system */
int interrupt_init(void);

/* Enable/disable interrupts */
void interrupt_enable(void);
void interrupt_disable(void);

/* Timer / clock interrupt */
void timer_init(uint32_t freq_hz);
void timer_set_oneshot(uint64_t ticks);
void timer_ack(void);

#endif /* _INTERRUPT_H_ */