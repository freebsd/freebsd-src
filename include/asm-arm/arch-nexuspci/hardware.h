/*
 * linux/include/asm-arm/arch-nexuspci/hardware.h
 *
 * Copyright (C) 1998, 1999, 2000 FutureTV Labs Ltd.
 *
 * This file contains the hardware definitions of the FTV PCI card.
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
 * 0xffe00000	0x20000000	INTCONT
 * 0xffd00000	0x30000000	Status
 * 0xffc00000	0x60000000	PLX registers
 * 0xfe000000	0xC0000000	PCI I/O
 * 0xfd000000	0x70000000	cache flush
 * 0xfc000000	0x80000000	PCI/ISA memory
 * 0xe0000000	0x10000000	SCC2691 DUART
 */

/*
 * Mapping areas
 */
#define INTCONT_BASE		0xffe00000
#define STATUS_BASE		0xffd00000
#define PLX_BASE		0xffc00000
#define PCIO_BASE		0xfe000000
#define FLUSH_BASE		0xfd000000
#define DUART_BASE		0xe0000000
#define PCIMEM_BASE		0xfc000000

#define PLX_IO_START		0xC0000000
#define PLX_MEM_START		0x80000000
#define PLX_START		0x60000000
#define STATUS_START		0x30000000
#define INTCONT_START		0x20000000
#define DUART_START		0x10000000

/*
 * RAM definitions
 */
#define RAM_BASE		0x40000000
#define FLUSH_BASE_PHYS		0x70000000

/*
 * Miscellaneous INTCONT bits
 */
#define INTCONT_FIQ_PLX		0x00
#define INTCONT_FIQ_D		0x02
#define INTCONT_FIQ_C		0x04
#define INTCONT_FIQ_B		0x06
#define INTCONT_FIQ_A		0x08
#define INTCONT_FIQ_SYSERR	0x0a
#define INTCONT_IRQ_DUART	0x0c
#define INTCONT_IRQ_PLX		0x0e
#define INTCONT_IRQ_D		0x10
#define INTCONT_IRQ_C		0x12
#define INTCONT_IRQ_B		0x14
#define INTCONT_IRQ_A		0x16
#define INTCONT_IRQ_SYSERR	0x1e

#define INTCONT_WATCHDOG	0x18
#define INTCONT_LED		0x1a
#define INTCONT_PCI_RESET	0x1c

#define UNCACHEABLE_ADDR	STATUS_BASE

#endif
