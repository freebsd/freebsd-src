/* 
 * arch/ppc/kernel/xics.h
 *
 * Copyright 2000 IBM Corporation.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#ifndef _PPC_KERNEL_XICS_H
#define _PPC_KERNEL_XICS_H

#include "local_irq.h"

extern struct hw_interrupt_type xics_pic;
extern struct hw_interrupt_type xics_8259_pic;

void xics_init_IRQ(void);
void xics_init_irq_desc(irq_desc_t *);
int xics_get_irq(struct pt_regs *);

#endif /* _PPC_KERNEL_XICS_H */
