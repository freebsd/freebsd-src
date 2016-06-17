/*
 * linux/include/asm-arm/arch-nexuspci/memory.h
 *
 * Copyright (c) 1997, 1998, 2000 FutureTV Labs Ltd.
 * Copyright (c) 1999 Russell King
 *
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
#define PHYS_OFFSET	(0x40000000UL)
#define BUS_OFFSET	(0xe0000000UL)

/*
 * DRAM is contiguous
 */
#define __virt_to_phys(vpage) ((unsigned long)(vpage) - PAGE_OFFSET + PHYS_OFFSET)
#define __phys_to_virt(ppage) ((unsigned long)(ppage) + PAGE_OFFSET - PHYS_OFFSET)
#define __virt_to_phys__is_a_macro
#define __phys_to_virt__is_a_macro

/*
 * On the PCI bus the DRAM appears at address 0xe0000000
 */
#define __virt_to_bus__is_a_macro
#define __virt_to_bus(x) ((unsigned long)(x) - PAGE_OFFSET + BUS_OFFSET)
#define __bus_to_virt__is_a_macro
#define __bus_to_virt(x) ((unsigned long)(x) + PAGE_OFFSET - BUS_OFFSET)

#endif
