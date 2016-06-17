/*
 * linux/include/asm-arm/arch-cl7500/memory.h
 *
 * Copyright (c) 1996,1997,1998 Russell King.
 *
 * Changelog:
 *  20-Oct-1996	RMK	Created
 *  31-Dec-1997	RMK	Fixed definitions to reduce warnings
 *  11-Jan-1998	RMK	Uninlined to reduce hits on cache
 *  08-Feb-1998	RMK	Added __virt_to_bus and __bus_to_virt
 *  21-Mar-1999	RMK	Renamed to memory.h
 *		RMK	Added TASK_SIZE and PAGE_OFFSET
 */
#ifndef __ASM_ARCH_MMU_H
#define __ASM_ARCH_MMU_H

/*
 * Task size: 3GB
 */
#define TASK_SIZE	(0xc0000000UL)
#define TASK_SIZE_26	(0x04000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)

/*
 * Page offset: 3GB
 */
#define PAGE_OFFSET	(0xc0000000UL)
#define PHYS_OFFSET	(0x10000000UL)

#define __virt_to_phys__is_a_macro
#define __virt_to_phys(vpage) ((vpage) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt__is_a_macro
#define __phys_to_virt(ppage) ((ppage) + PAGE_OFFSET - PHYS_OFFSET)

/*
 * These are exactly the same on the RiscPC as the
 * physical memory view.
 */
#define __virt_to_bus__is_a_macro
#define __virt_to_bus(x) __virt_to_phys(x)
#define __bus_to_virt__is_a_macro
#define __bus_to_virt(x) __phys_to_virt(x)

#endif
