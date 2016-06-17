/*
 * include/asm-ppc/lopec_serial.h
 *
 * Definitions for Motorola LoPEC board.
 *
 * Author: Dan Cox
 *         danc@mvista.com (or, alternately, source@mvista.com)
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __PLATFORMS_LOPEC_SERIAL_H__
#define __PLATFORMS_LOPEC_SERIAL_H__

#define RS_TABLE_SIZE	3

#define BASE_BAUD	(1843200 / 16)
#define UART0_INT	29
#define UART1_INT	20
#define UART2_INT	21

#define UART0_PORT	0xFFE10000
#define UART1_PORT	0xFFE11000
#define UART2_PORT	0xFFE12000
#define UART0_IO_BASE	(u8 *) UART0_PORT
#define UART1_IO_BASE	(u8 *) UART1_PORT
#define UART2_IO_BASE	(u8 *) UART2_PORT

#define STD_UART_OP(num)					\
	{ 0, BASE_BAUD, UART##num##_PORT, UART##num##_INT,	\
		ASYNC_BOOT_AUTOCONF,				\
		iomem_base: UART##num##_IO_BASE,		\
		io_type: SERIAL_IO_MEM },

#define SERIAL_PORT_DFNS 	\
		STD_UART_OP(0)	\
		STD_UART_OP(1)	\
		STD_UART_OP(2)

#endif /* __PLATFORMS_LOPEC_SERIAL_H__ */
