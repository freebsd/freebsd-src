/*
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

/*
 * MMU_init_hw does the chip-specific initialization of the MMU hardware.
 */
void __init MMU_init_hw(void)
{
	/*
	 * The Zone Protection Register (ZPR) defines how protection will
	 * be applied to every page which is a member of a given zone. At
	 * present, we utilize only two of the 4xx's zones.
	 * The zone index bits (of ZSEL) in the PTE are used for software
	 * indicators, except the LSB.  For user access, zone 1 is used,
	 * for kernel access, zone 0 is used.  We set all but zone 1
	 * to zero, allowing only kernel access as indicated in the PTE.
	 * For zone 1, we set a 01 binary (a value of 10 will not work)
	 * to allow user access as indicated in the PTE.  This also allows
	 * kernel access as indicated in the PTE.
	 */

        mtspr(SPRN_ZPR, 0x10000000);

	flush_instruction_cache();

	/*
	 * Set up the real-mode cache parameters for the exception vector
	 * handlers (which are run in real-mode).
	 */

        mtspr(SPRN_DCWR, 0x00000000);	/* All caching is write-back */

        /*
	 * Cache instruction and data space where the exception
	 * vectors and the kernel live in real-mode.
	 */
	/*
	 * Once the following code is enhanced to not assume that it should
	 * just enable caching on the first 512MB, we need to make sure that
	 * we either are given the cache in a known state or handle correctly
	 * the cache being enabled previously.  Currently this will clear
	 * without flushing. -- Tom
	 */

        mtspr(SPRN_DCCR, 0xF0000000);	/* 512 MB of data space at 0x0. */
        mtspr(SPRN_ICCR, 0xF0000000);	/* 512 MB of instr. space at 0x0. */
}
