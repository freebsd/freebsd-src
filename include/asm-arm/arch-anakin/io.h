/*
 *  linux/include/asm-arm/arch-anakin/io.h
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   10-Apr-2001 TTC	Created
 */

#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H


#define IO_SPACE_LIMIT		0xffffffff

#define __io(a)			a
#define __arch_getw(a)		(*(volatile unsigned short *) (a))
#define __arch_putw(b, a)	(*(volatile unsigned short *) (a) = (b))

#define iomem_valid_addr(i, s)	1
#define iomem_to_phys(i)	i

/*
 * We don't support ins[lb]/outs[lb].  Make them fault.
 */
#define __raw_readsb(p,d,l)	do { *(int *)0 = 0; } while (0)
#define __raw_readsl(p,d,l)	do { *(int *)0 = 0; } while (0)
#define __raw_writesb(p,d,l)	do { *(int *)0 = 0; } while (0)
#define __raw_writesl(p,d,l)	do { *(int *)0 = 0; } while (0)

#endif
