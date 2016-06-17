/*
 *  linux/include/asm-arm/arch-anakin/vmalloc.h
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   09-Apr-2001 TTC	Created
 */

#ifndef __ASM_ARCH_VMALLOC_H
#define __ASM_ARCH_VMALLOC_H

/*
 * VMALLOC_ARCH_OFFSET must be set to VMALLOC_OFFSET (check
 * linux/arch/arm/kernel/traps.c)
 */
#define VMALLOC_ARCH_OFFSET	(8 * 1024 * 1024)
#define VMALLOC_VMADDR(a)	((unsigned int) (a))
#define VMALLOC_START		((VMALLOC_VMADDR(high_memory) + VMALLOC_ARCH_OFFSET) & ~(VMALLOC_ARCH_OFFSET - 1))
#define VMALLOC_END		(PAGE_OFFSET + 0x10000000)

#endif
