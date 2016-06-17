/*
 * FILE NAME
 *	include/asm-mips/vr41xx/tb0229.h
 *
 * BRIEF MODULE DESCRIPTION
 *	Include file for TANBAC TB0229 and TB0219.
 *
 * Copyright 2002,2003 Yoichi Yuasa
 *                yuasa@hh.iij4u.or.jp
 *
 * Modified for TANBAC TB0229:
 * Copyright 2003 Megasolution Inc.
 *                matsu@megasolution.jp
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 */
#ifndef __TANBAC_TB0229_H
#define __TANBAC_TB0229_H

#include <asm/addrspace.h>
#include <asm/vr41xx/vr41xx.h>

/*
 * Board specific address mapping
 */
#define VR41XX_PCI_MEM1_BASE		0x10000000
#define VR41XX_PCI_MEM1_SIZE		0x04000000
#define VR41XX_PCI_MEM1_MASK		0x7c000000

#define VR41XX_PCI_MEM2_BASE		0x14000000
#define VR41XX_PCI_MEM2_SIZE		0x02000000
#define VR41XX_PCI_MEM2_MASK		0x7e000000

#define VR41XX_PCI_IO_BASE		0x16000000
#define VR41XX_PCI_IO_SIZE		0x02000000
#define VR41XX_PCI_IO_MASK		0x7e000000

#define VR41XX_PCI_IO_START		0x01000000
#define VR41XX_PCI_IO_END		0x01ffffff

#define VR41XX_PCI_MEM_START		0x12000000
#define VR41XX_PCI_MEM_END		0x15ffffff

#define IO_PORT_BASE			KSEG1ADDR(VR41XX_PCI_IO_BASE)
#define IO_PORT_RESOURCE_START		0
#define IO_PORT_RESOURCE_END		VR41XX_PCI_IO_SIZE
#define IO_MEM1_RESOURCE_START		VR41XX_PCI_MEM1_BASE
#define IO_MEM1_RESOURCE_END		(VR41XX_PCI_MEM1_BASE + VR41XX_PCI_MEM1_SIZE)
#define IO_MEM2_RESOURCE_START		VR41XX_PCI_MEM2_BASE
#define IO_MEM2_RESOURCE_END		(VR41XX_PCI_MEM2_BASE + VR41XX_PCI_MEM2_SIZE)

/*
 * General-Purpose I/O Pin Number
 */
#define TB0219_PCI_SLOT1_PIN		2
#define TB0219_PCI_SLOT2_PIN		3
#define TB0219_PCI_SLOT3_PIN		4

/*
 * Interrupt Number
 */
#define TB0219_PCI_SLOT1_IRQ		GIU_IRQ(TB0219_PCI_SLOT1_PIN)
#define TB0219_PCI_SLOT2_IRQ		GIU_IRQ(TB0219_PCI_SLOT2_PIN)
#define TB0219_PCI_SLOT3_IRQ		GIU_IRQ(TB0219_PCI_SLOT3_PIN)

#define TB0219_RESET_REGS		KSEG1ADDR(0x0a00000e)

extern void tanbac_tb0229_restart(char *command);

#endif /* __TANBAC_TB0229_H */
