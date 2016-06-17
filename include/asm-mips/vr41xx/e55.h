/*
 * FILE NAME
 *	include/asm-mips/vr41xx/e55.h
 *
 * BRIEF MODULE DESCRIPTION
 *	Include file for CASIO CASSIOPEIA E-10/15/55/65.
 *
 * Copyright 2002 Yoichi Yuasa
 *                yuasa@hh.iij4u.or.jp
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 */
#ifndef __CASIO_E55_H
#define __CASIO_E55_H

#include <asm/addrspace.h>
#include <asm/vr41xx/vr41xx.h>

/*
 * Board specific address mapping
 */
#define VR41XX_ISA_MEM_BASE		0x10000000
#define VR41XX_ISA_MEM_SIZE		0x04000000

#define VR41XX_ISA_IO_BASE		0x14000000
#define VR41XX_ISA_IO_SIZE		0x04000000

#define IO_PORT_BASE			KSEG1ADDR(VR41XX_ISA_IO_BASE)
#define IO_PORT_RESOURCE_START		0
#define IO_PORT_RESOURCE_END		VR41XX_ISA_IO_SIZE
#define IO_MEM_RESOURCE_START		VR41XX_ISA_MEM_BASE
#define IO_MEM_RESOURCE_END		(VR41XX_ISA_MEM_BASE + VR41XX_ISA_MEM_SIZE)

#endif /* __CASIO_E55_H */
