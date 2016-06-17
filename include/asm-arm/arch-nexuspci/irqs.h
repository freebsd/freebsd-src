/*
 * linux/include/asm-arm/arch-nexuspci/irqs.h
 *
 * Copyright (C) 1997, 1998, 2000 Philip Blundell
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

/*
 * The hardware is capable of routing any interrupt source (except the
 * DUART) to either IRQ or FIQ.  We ignore FIQ and use IRQ exclusively
 * for simplicity.  
 */

#define IRQ_DUART		0
#define IRQ_PLX 		1
#define IRQ_PCI_D		2
#define IRQ_PCI_C		3
#define IRQ_PCI_B		4
#define IRQ_PCI_A	        5
#define IRQ_SYSERR		6	/* only from IOSLAVE rev B */

#define FIRST_IRQ		IRQ_DUART
#define LAST_IRQ		IRQ_SYSERR

/* timer is part of the DUART */
#define IRQ_TIMER		IRQ_DUART

#define irq_cannonicalize(i)	(i)
