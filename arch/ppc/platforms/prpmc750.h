/*
 * include/asm-ppc/platforms/prpmc750.h
 * 
 * Definitions for Motorola PrPMC750 board support
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
#ifndef __ASM_PRPMC750_H__
#define __ASM_PRPMC750_H__

#include <linux/serial_reg.h>

#define PRPMC750_PCI_CONFIG_ADDR	0x80000cf8
#define PRPMC750_PCI_CONFIG_DATA	0x80000cfc

#define PRPMC750_PCI_PHY_MEM_BASE	0xc0000000
#define PRPMC750_PCI_MEM_BASE		0xf0000000
#define PRPMC750_PCI_IO_BASE		0x80000000

#define PRPMC750_ISA_IO_BASE		PRPMC750_PCI_IO_BASE
#define PRPMC750_ISA_MEM_BASE		PRPMC750_PCI_MEM_BASE
#define PRPMC750_PCI_MEM_OFFSET		PRPMC750_PCI_PHY_MEM_BASE

#define PRPMC750_SYS_MEM_BASE		0x80000000

#define PRPMC750_PCI_LOWER_MEM		0x00000000
#define PRPMC750_PCI_UPPER_MEM_AUTO	0x3bf7ffff
#define PRPMC750_PCI_UPPER_MEM		0x3bffffff
#define PRPMC750_PCI_LOWER_IO		0x00000000
#define PRPMC750_PCI_UPPER_IO		0x0ff7ffff

#define PRPMC750_HAWK_MPIC_BASE		0xfbf80000
#define PRPMC750_HAWK_SMC_BASE		0xfef80000

#define PRPMC750_BASE_BAUD		1843200
#define PRPMC750_SERIAL_0		0xfef88000
#define PRPMC750_SERIAL_0_DLL		(PRPMC750_SERIAL_0 + (UART_DLL << 4))
#define PRPMC750_SERIAL_0_DLM		(PRPMC750_SERIAL_0 + (UART_DLM << 4))
#define PRPMC750_SERIAL_0_LCR		(PRPMC750_SERIAL_0 + (UART_LCR << 4))

#define PRPMC750_STATUS_REG		0xfef88080
#define PRPMC750_BAUDOUT_MASK		0x02
#define PRPMC750_MONARCH_MASK		0x01

#define PRPMC750_MODRST_REG		0xfef880a0
#define PRPMC750_MODRST_MASK		0x01

#define PRPMC750_PIRQ_REG		0xfef880b0
#define PRPMC750_SEL1_MASK		0x02
#define PRPMC750_SEL0_MASK		0x01

#define PRPMC750_TBEN_REG		0xfef880c0
#define PRPMC750_TBEN_MASK		0x01

#endif /* __ASM_PRPMC750_H__ */
#endif /* __KERNEL__ */
