/*
 *  linux/include/asm-arm/arch-anakin/irqs.h
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   10-Apr-2001 TTC	Created
 */

#ifndef __ASM_ARCH_IRQS_H
#define __ASM_ARCH_IRQS_H

#define NR_IRQS			16

#define IRQ_UART0		0
#define IRQ_UART1		1
#define IRQ_UART2		2
#define IRQ_TICK		3
#define IRQ_CODEC		4
#define IRQ_UART4		5
#define IRQ_TOUCHSCREEN		6
#define IRQ_UART3		7
#define IRQ_FIFO		8
#define IRQ_CAN			9
#define IRQ_COMPACTFLASH	10
#define IRQ_BOSH		12
#define IRQ_ANAKIN		15

#endif
