/*
 * linux/include/asm-arm/arch-cl7500/serial.h
 *
 * Copyright (c) 1996 Russell King.
 * Copyright (C) 1999 Nexus Electronics Ltd.
 *
 * Changelog:
 *  15-10-1996	RMK	Created
 *  10-08-1999	PJB	Added COM3/COM4 for CL7500
 */
#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <asm/arch/hardware.h>

/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD (1843200 / 16)

#define RS_TABLE_SIZE	16

#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

     /* UART CLK        PORT  IRQ     FLAGS        */
#define STD_SERIAL_PORT_DEFNS \
	{ 0, BASE_BAUD, 0x3F8, 10, STD_COM_FLAGS },	/* ttyS0 */	\
	{ 0, BASE_BAUD, 0x2F8,  0, STD_COM_FLAGS },	/* ttyS1 */     \
        /* ISA Slot Serial ports */ \
	{ 0, BASE_BAUD, ISASLOT_IO + 0x2e8,  41, STD_COM_FLAGS },	/* ttyS2 */	\
	{ 0, BASE_BAUD, ISASLOT_IO + 0x3e8,  40, STD_COM_FLAGS },	/* ttyS3 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS }, 	/* ttyS4 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS },	/* ttyS5 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS },	/* ttyS6 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS },	/* ttyS7 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS },	/* ttyS8 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS },	/* ttyS9 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS },	/* ttyS10 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS },	/* ttyS11 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS },	/* ttyS12 */	\
	{ 0, BASE_BAUD, 0    ,  0, STD_COM_FLAGS },	/* ttyS13 */

#define EXTRA_SERIAL_PORT_DEFNS

#endif
