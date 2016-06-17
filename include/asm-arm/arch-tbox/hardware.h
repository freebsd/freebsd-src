/*
 * linux/include/asm-arm/arch-tbox/hardware.h
 *
 * Copyright (C) 1998, 1999, 2000 Philip Blundell
 * Copyright (C) 2000 FutureTV Labs Ltd
 *
 * This file contains the hardware definitions of the Tbox
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

/*    Logical    Physical
 * 0xfff00000	0x00100000	I/O
 * 0xfff00000	0x00100000	  Expansion CS0
 * 0xfff10000	0x00110000	  DMA
 * 0xfff20000	0x00120000	  C-Cube
 * 0xfff30000	0x00130000	  FPGA 1
 * 0xfff40000	0x00140000	  UART 2
 * 0xfff50000	0x00150000	  UART 1
 * 0xfff60000	0x00160000	  CS8900
 * 0xfff70000	0x00170000	  INTCONT
 * 0xfff80000	0x00180000	  RAMDAC
 * 0xfff90000	0x00190000	  Control 0
 * 0xfffa0000	0x001a0000	  Control 1
 * 0xfffb0000	0x001b0000	  Control 2
 * 0xfffc0000	0x001c0000	  FPGA 2
 * 0xfffd0000	0x001d0000	  INTRESET
 * 0xfffe0000	0x001e0000	  C-Cube DMA throttle
 * 0xffff0000	0x001f0000	  Expansion CS1
 * 0xffe00000	0x82000000	cache flush
 */

/*
 * Mapping areas
 */
#define IO_BASE			0xfff00000
#define IO_START		0x00100000
#define FLUSH_BASE		0xffe00000

#define INTCONT			0xfff70000

#define FPGA1CONT		0xffff3000

/*
 * RAM definitions
 */
#define RAM_BASE		0x80000000
#define FLUSH_BASE_PHYS		0x82000000

#define UNCACHEABLE_ADDR	INTCONT

#endif
