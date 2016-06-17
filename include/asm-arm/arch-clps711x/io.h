/*
 *  linux/include/asm-arm/arch-clps711x/io.h
 *
 *  Copyright (C) 1999 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

#define __io(a)			((a))
#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		((unsigned long)(a))

/*
 * Generic virtual read/write
 */
#define __arch_getw(a)		(*(volatile unsigned short *)(a))
#define __arch_putw(v,a)	(*(volatile unsigned short *)(a) = (v))

/*
 * ioremap support - validate a PCI memory address, and convert it
 * to a physical address for the page tables.
 */
#define iomem_valid_addr(iomem,size)	(1)
#define iomem_to_phys(iomem)		(iomem)

/*
 * We don't support ins[lb]/outs[lb].  Make them fault.
 */
#define __raw_readsb(p,d,l)	do { *(int *)0 = 0; } while (0)
#define __raw_readsl(p,d,l)	do { *(int *)0 = 0; } while (0)
#define __raw_writesb(p,d,l)	do { *(int *)0 = 0; } while (0)
#define __raw_writesl(p,d,l)	do { *(int *)0 = 0; } while (0)

#endif
