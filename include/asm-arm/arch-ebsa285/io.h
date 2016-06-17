/*
 *  linux/include/asm-arm/arch-ebsa285/io.h
 *
 *  Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Modifications:
 *   06-12-1997	RMK	Created.
 *   07-04-1999	RMK	Major cleanup
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffff

/*
 * Translation of various region addresses to virtual addresses
 */
#define __io(a)			(PCIO_BASE + (a))
#if 1
#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		(PCIMEM_BASE + (unsigned long)(a))
#else

static inline unsigned long ___mem_pci(unsigned long a)
{
	if (a <= 0xc0000000 || a >= 0xe0000000)
		BUG();
	return a;
}

static inline unsigned long ___mem_isa(unsigned long a)
{
	if (a >= 16*1048576)
		BUG();
	return PCIMEM_BASE + a;
}
#define __mem_pci(a)		___mem_pci((unsigned long)(a))
#define __mem_isa(a)		___mem_isa((unsigned long)(a))
#endif

/*
 * Generic virtual read/write
 */
#define __arch_getw(a)		(*(volatile unsigned short *)(a))
#define __arch_putw(v,a)	(*(volatile unsigned short *)(a) = (v))

#define iomem_valid_addr(iomem,sz)	(1)
#define iomem_to_phys(iomem)		(iomem)

#endif
