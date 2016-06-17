/*
 * include/asm-ppc/sandpoint_serial.h
 * 
 * Definitions for Motorola SPS Sandpoint Test Platform
 *
 * Author: Mark A. Greer
 *         mgreer@mvista.com
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __ASMPPC_SANDPOINT_SERIAL_H
#define __ASMPPC_SANDPOINT_SERIAL_H

#include <linux/config.h>

#define SANDPOINT_SERIAL_0		0xfe0003f8
#define SANDPOINT_SERIAL_1		0xfe0002f8

#ifdef CONFIG_SERIAL_MANY_PORTS
#define RS_TABLE_SIZE  64
#else
#define RS_TABLE_SIZE  2
#endif

/* Rate for the 1.8432 Mhz clock for the onboard serial chip */
#define BASE_BAUD ( 1843200 / 16 )

#define STD_SERIAL_PORT_DFNS \
	/* ttyS0 */							\
        { 0, BASE_BAUD, SANDPOINT_SERIAL_0, 4, ASYNC_BOOT_AUTOCONF,	\
		iomem_base: (u8 *)SANDPOINT_SERIAL_0,			\
		io_type: SERIAL_IO_MEM },				\
	/* ttyS1 */							\
        { 0, BASE_BAUD, SANDPOINT_SERIAL_1, 3, ASYNC_BOOT_AUTOCONF,	\
		iomem_base: (u8 *)SANDPOINT_SERIAL_1,			\
		io_type: SERIAL_IO_MEM },

#define SERIAL_PORT_DFNS \
        STD_SERIAL_PORT_DFNS

#endif /* __ASMPPC_SANDPOINT_SERIAL_H */
