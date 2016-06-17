/*
 * Modifications by Matt Porter (mporter@mvista.com) to support
 * PPC44x Book E processors.
 *
 * This file contains the routines for initializing the MMU
 * on the 4xx series of chips.
 *  -- paulus
 * 
 *  Derived from arch/ppc/mm/init.c:
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Modifications by Paul Mackerras (PowerMac) (paulus@cs.anu.edu.au)
 *  and Cort Dougan (PReP) (cort@cs.nmt.edu)
 *    Copyright (C) 1996 Paul Mackerras
 *  Amiga/APUS changes by Jesper Skov (jskov@cygnus.co.uk).
 *
 *  Derived from "arch/i386/mm/init.c"
 *    Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#include <linux/config.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/highmem.h>

#include <asm/pgalloc.h>
#include <asm/prom.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/mmu.h>
#include <asm/uaccess.h>
#include <asm/smp.h>
#include <asm/bootx.h>
#include <asm/machdep.h>
#include <asm/setup.h>
#include <asm/ibm44x.h>

#include "mmu_decl.h"
#include "mem_pieces.h"

extern char etext[], _stext[];
extern struct mem_pieces phys_avail;

/* Used by the 44x TLB replacement exception handler.
 * Just needed it declared someplace.
 */
unsigned int tlb_44x_index = 0;
unsigned int tlb_44x_hwater = 61;

/*
 * "Pins" a 256MB TLB entry in AS0 for kernel lowmem
 */
static void __init
ppc44x_pin_tlb(int slot, unsigned int virt, unsigned int phys)
{
	unsigned long attrib;

	__asm__ __volatile__("\
	clrrwi	%2,%2,10\n\
	ori	%2,%2,%4\n\
	clrrwi	%1,%1,10\n\
	li	%0,0\n\
	ori	%0,%0,%5\n\
	tlbwe	%2,%3,%6\n\
	tlbwe	%1,%3,%7\n\
	tlbwe	%0,%3,%8"
	:
	: "r" (attrib), "r" (phys), "r" (virt), "r" (slot),
	  "i" (PPC44x_TLB_VALID | PPC44x_TLB_PAGESZ(PPC44x_PAGESZ_256M)),
	  "i" (PPC44x_TLB_SW | PPC44x_TLB_SR | PPC44x_TLB_SX | PPC44x_TLB_G),
	  "i" (PPC44x_TLB_PAGEID),
	  "i" (PPC44x_TLB_XLAT),
	  "i" (PPC44x_TLB_ATTRIB));
}

/*
 * Configure PPC44x TLB for AS0 exception processing.
 */
static void __init
ppc44x_tlb_config(void)
{
	unsigned int pinned_tlbs = 1;
	int i;

	/*
	 * If lowmem is not on a pin tlb entry size boundary,
	 * then reserve the last page of system memory. This
	 * eliminates the possibility of a speculative dcache
	 * fetch past the end of system memory that would
	 * result in a machine check exception.
	 */
	if (total_lowmem & (PPC44x_PIN_SIZE - 1))
		mem_pieces_remove(&phys_avail, total_lowmem - PAGE_SIZE, PAGE_SIZE, 1);

	/* Determine number of entries necessary to cover lowmem */
	pinned_tlbs = (unsigned int)
		(_ALIGN(total_lowmem, PPC44x_PIN_SIZE) >> PPC44x_PIN_SHIFT);

	/* Write upper watermark to save location */
	tlb_44x_hwater = PPC44x_LOW_SLOT - pinned_tlbs;

	/* If necessary, set additional pinned TLBs */
	if (pinned_tlbs > 1)
		for (i = (PPC44x_LOW_SLOT-(pinned_tlbs-1)); i < PPC44x_LOW_SLOT; i++) {
			unsigned int phys_addr = (PPC44x_LOW_SLOT-i) * PPC44x_PIN_SIZE;
			ppc44x_pin_tlb(i, phys_addr+PAGE_OFFSET, phys_addr);
		}

	/* Make sure vmalloc doesn't use virtual space covered by
	   the last pinned TLB entry. */
	vmalloc_start = KERNELBASE + _ALIGN(total_lowmem, PPC44x_PIN_SIZE);
}

/*
 * MMU_init_hw does the chip-specific initialization of the MMU hardware.
 */
void __init MMU_init_hw(void)
{
	flush_instruction_cache();

	ppc44x_tlb_config();
}
