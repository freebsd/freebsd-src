/*
 * linux/include/asm-arm/arch-tbox/irqs.h
 *
 * Copyright (C) 1998, 2000 Philip Blundell
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define IRQ_MPEGDMA		0
#define IRQ_ASHTX		1
#define IRQ_ASHRX		2
#define IRQ_VSYNC		3
#define IRQ_HSYNC		4
#define IRQ_MPEG		5
#define IRQ_UART2		6
#define IRQ_UART1		7
#define IRQ_ETHERNET		8
#define IRQ_TIMER		9
#define IRQ_AUDIODMA		10
/* bit 11 used for video field ident */
#define IRQ_EXPMODCS0		12
#define IRQ_EXPMODCS1		13

#define irq_cannonicalize(i)	(i)
