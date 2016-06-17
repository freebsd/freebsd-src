/*
 * include/asm-ppc/platforms/prpmc750_serial.h
 * 
 * Motorola PrPMC750 serial support
 *
 * Author: Matt Porter <mporter@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifdef __KERNEL__
#ifndef __ASM_PRPMC750_SERIAL_H__
#define __ASM_PRPMC750_SERIAL_H__

#include <linux/config.h>
#include <platforms/prpmc750.h>

#define RS_TABLE_SIZE  4

/* Rate for the 1.8432 Mhz clock for the onboard serial chip */
#define BASE_BAUD  (PRPMC750_BASE_BAUD / 16)

#ifndef SERIAL_MAGIC_KEY
#define kernel_debugger ppc_kernel_debug
#endif

#define SERIAL_PORT_DFNS \
        { 0, BASE_BAUD, PRPMC750_SERIAL_0, 1, ASYNC_SKIP_TEST, \
		iomem_base: (unsigned char *)PRPMC750_SERIAL_0, \
		iomem_reg_shift: 4, \
		io_type: SERIAL_IO_MEM } /* ttyS0 */

#endif /* __ASM_PRPMC750_SERIAL_H__ */
#endif /* __KERNEL__ */
