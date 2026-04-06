/*
 * Interrupt Handling Framework
 * uOS(m) - User OS Mobile
 */

#ifndef _INTERRUPT_H_
#define _INTERRUPT_H_

#include <stdint.h>

/* Interrupt handler function type */
typedef void (*interrupt_handler_t)(void);

/* Register an interrupt handler */
int interrupt_register_handler(uint32_t irq, interrupt_handler_t handler);

/* Initialize interrupt system */
int interrupt_init(void);

/* Enable/disable interrupts */
void interrupt_enable(void);
void interrupt_disable(void);

#endif /* _INTERRUPT_H_ */