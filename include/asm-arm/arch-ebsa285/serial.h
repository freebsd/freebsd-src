/*
 *  linux/include/asm-arm/arch-ebsa285/serial.h
 *
 *  Copyright (C) 1996,1997,1998 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   15-10-1996	RMK	Created
 *   25-05-1998	PJB	CATS support
 */
#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <asm/irq.h>

/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD (1843200 / 16)

#define _SER_IRQ0	IRQ_ISA_UART
#define _SER_IRQ1	IRQ_ISA_UART2

#define RS_TABLE_SIZE	16

#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

     /* UART CLK        PORT  IRQ     FLAGS        */
#define STD_SERIAL_PORT_DEFNS \
	{ 0, BASE_BAUD, 0x3F8, _SER_IRQ0, STD_COM_FLAGS },	/* ttyS0 */	\
	{ 0, BASE_BAUD, 0x2F8, _SER_IRQ1, STD_COM_FLAGS },	/* ttyS1 */

#define EXTRA_SERIAL_PORT_DEFNS

#endif
