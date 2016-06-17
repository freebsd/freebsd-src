/*
 * FILE NAME
 *	include/asm-mips/vr41xx/mpc30x.h
 *
 * BRIEF MODULE DESCRIPTION
 *	Include file for Victor MP-C303/304.
 *
 * Copyright 2002,2003 Yoichi Yuasa
 *                yuasa@hh.iij4u.or.jp
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 */
#ifndef __VICTOR_MPC30X_H
#define __VICTOR_MPC30X_H

#include <linux/config.h>

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
#define VRC4173_PIN			1
#define MQ200_PIN			4

/*
 * Interrupt Number
 */
#define VRC4173_CASCADE_IRQ		GIU_IRQ(VRC4173_PIN)
#define MQ200_IRQ			GIU_IRQ(MQ200_PIN)

#endif /* __VICTOR_MPC30X_H */
