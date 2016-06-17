/*
 * linux/include/asm-arm/arch-tbox/serial.h
 *
 * Copyright (c) 1996 Russell King.
 * Copyright (c) 1998 Phil Blundell
 *
 * Changelog:
 *  15-10-1996	RMK	Created
 *  09-06-1998  PJB	tbox version
 */
#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD (1843200 / 16)

#define RS_TABLE_SIZE	2

#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

     /* UART CLK        PORT  IRQ     FLAGS        */
#define STD_SERIAL_PORT_DEFNS \
	{ 0, BASE_BAUD, 0xffff4000 >> 2, 6, STD_COM_FLAGS }, /* ttyS0 */ \
	{ 0, BASE_BAUD, 0xffff5000 >> 2, 7, STD_COM_FLAGS }, /* ttyS1 */

#define EXTRA_SERIAL_PORT_DEFNS

#endif
