/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh5/kernel/head.S
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 *
 * benedict.gaster@superh.com:	 2nd May 2002
 *    Moved definition of empty_zero_page to its own section allowing
 *    it to be placed at an absolute address known at load time.
 *
 * lethal@linux-sh.org:          9th May 2003
 *    Kill off GLOBAL_NAME() usage.
 */

#include <linux/config.h>

#include <asm/page.h>
#include <asm/mmu_context.h>
#include <asm/cache.h>
#include <asm/tlb.h>
#include <asm/processor.h>
#include <asm/registers.h>

/*
 * MMU defines: TLB boundaries.
 */

#define MMUIR_FIRST	ITLB_FIXED	
#define MMUIR_END	ITLB_LAST_VAR_UNRESTRICTED+TLB_STEP
#define MMUIR_STEP	TLB_STEP

#define MMUDR_FIRST	DTLB_FIXED
#define MMUDR_END	DTLB_LAST_VAR_UNRESTRICTED+TLB_STEP
#define MMUDR_STEP	TLB_STEP

/*
 * MMU defines: Fixed TLBs.
 */
#define MMUIR_TEXT_H	0x0000000000000003 | (CONFIG_CACHED_MEMORY_OFFSET + CONFIG_MEMORY_START)
			/* Enabled, Shared, ASID 0, Eff. Add. 0xA0000000 */

#define MMUIR_TEXT_L	0x000000000000009a | (CONFIG_MEMORY_START)
			/* 512 Mb, Cacheable, Write-back, execute, Not User, Ph. Add. */

#define MMUDR_CACHED_H	0x0000000000000003 | (CONFIG_CACHED_MEMORY_OFFSET + CONFIG_MEMORY_START)
			/* Enabled, Shared, ASID 0, Eff. Add. 0xA0000000 */
#define MMUDR_CACHED_L	0x000000000000015a | (CONFIG_MEMORY_START)
			/* 512 Mb, Cacheable, Write-back, read/write, Not User, Ph. Add. */

#ifdef CONFIG_ICACHE_DISABLED
#define	ICCR0_INIT_VAL	ICCR0_OFF			/* ICACHE off */
#else
#define	ICCR0_INIT_VAL	ICCR0_ON | ICCR0_ICI		/* ICE + ICI */
#endif
#define	ICCR1_INIT_VAL	ICCR1_NOLOCK			/* No locking */

#if defined (CONFIG_DCACHE_DISABLED)
#define	OCCR0_INIT_VAL	OCCR0_OFF			   /* D-cache: off  */
#elif defined (CONFIG_DCACHE_WRITE_THROUGH)
#define	OCCR0_INIT_VAL	OCCR0_ON | OCCR0_OCI | OCCR0_WT	   /* D-cache: on,   */
							   /* WT, invalidate */
#elif defined (CONFIG_DCACHE_WRITE_BACK)
#define	OCCR0_INIT_VAL	OCCR0_ON | OCCR0_OCI | OCCR0_WB	   /* D-cache: on,   */
							   /* WB, invalidate */
#else
#error preprocessor flag CONFIG_DCACHE_... not recognized!
#endif

#define	OCCR1_INIT_VAL	OCCR1_NOLOCK			   /* No locking     */

	.section	.empty_zero_page, "aw"
	.global empty_zero_page
	
empty_zero_page:	
	.long	1		/* MOUNT_ROOT_RDONLY */
	.long	0		/* RAMDISK_FLAGS */
	.long	0x0200		/* ORIG_ROOT_DEV */
	.long	1		/* LOADER_TYPE */
	.long	0x00360000	/* INITRD_START */
	.long	0x000a0000	/* INITRD_SIZE */
	.long	0

	.text	
	.balign 4096,0,4096	

	.section	.data, "aw"
	.balign	PAGE_SIZE

	.section	.data, "aw"
	.balign	PAGE_SIZE

	.global swapper_pg_dir
swapper_pg_dir:
	.space PAGE_SIZE, 0

	.global empty_bad_page
empty_bad_page:
	.space PAGE_SIZE, 0

	.global empty_bad_pte_table
empty_bad_pte_table:
	.space PAGE_SIZE, 0

variables:
	.global	fpu_in_use
fpu_in_use:	.quad	0


	.section	.text, "ax"
	.balign L1_CACHE_BYTES
/*
 * Condition at the entry of __stext:
 * . Reset state:
 *   . SR.FD    = 1		(FPU disabled)
 *   . SR.BL    = 1		(Exceptions disabled)
 *   . SR.MD    = 1		(Privileged Mode)
 *   . SR.MMU   = 0		(MMU Disabled)
 *   . SR.CD    = 0		(CTC User Visible)
 *   . SR.IMASK = Undefined	(Interrupt Mask)
 *
 * Operations supposed to be performed by __stext:
 * . prevent speculative fetch onto device memory while MMU is off
 * . reflect as much as possible SH5 ABI (r15, r26, r27, r18)
 * . first, save CPU state and set it to something harmless
 * . any CPU detection and/or endianness settings (?)
 * . initialize EMI/LMI (but not TMU/RTC/INTC/SCIF): TBD
 * . set initial TLB entries for cached and uncached regions
 *   (no fine granularity paging)
 * . set initial cache state
 * . enable MMU and caches
 * . set CPU to a consistent state
 *   . registers (including stack pointer and current/KCR0)
 *   . NOT expecting to set Exception handling nor VBR/RESVEC/DCR
 *     at this stage. This is all to later Linux initialization steps.
 *   . initialize FPU
 * . clear BSS
 * . jump into start_kernel()
 * . be prepared to hopeless start_kernel() returns.
 *
 */
	.global _stext
_stext:
	/*
	 * Prevent speculative fetch on device memory due to
	 * uninitialized target registers.
	 */
	ptabs/u	ZERO, t0
	ptabs/u	ZERO, t1
	ptabs/u	ZERO, t2
	ptabs/u	ZERO, t3
	ptabs/u	ZERO, t4
	ptabs/u	ZERO, t5
	ptabs/u	ZERO, t6
	ptabs/u	ZERO, t7
	synci

	/*
	 * Set variable/constant pointers according to SH5 ABI.
	 */
	_loada	constants, GCDT

	_loada	variables, GVDT

	/*
	 * Read/Set CPU state. After this block:
	 * r29 = Initial SR
	 */
	getcon	SR, r29
	movi	SR_HARMLESS, r20
	putcon	r20, SR

	/*
	 * Initialize EMI/LMI. To Be Done.
	 */

	/*
	 * CPU detection and/or endianness settings (?). To Be Done.
	 * Pure PIC code here, please ! Just save state into r30.
         * After this block:
	 * r30 = CPU type/Platform Endianness
	 */

	/*
	 * Set initial TLB entries for cached and uncached regions.
	 * Note: PTA/BLINK is PIC code, PTABS/BLINK isn't !
	 */
	/* Clear ITLBs */
	_ptar	clear_ITLB, t1
	movi	MMUIR_FIRST, r21
	movi	MMUIR_END, r22
clear_ITLB:
	putcfg	r21, 0, ZERO		/* Clear MMUIR[n].PTEH.V */
	addi	r21, MMUIR_STEP, r21
        bne	r21, r22, t1

	/* Clear DTLBs */
	_ptar	clear_DTLB, t1
	movi	MMUDR_FIRST, r21
	movi	MMUDR_END, r22
clear_DTLB:
	putcfg	r21, 0, ZERO		/* Clear MMUDR[n].PTEH.V */
	addi	r21, MMUDR_STEP, r21
        bne	r21, r22, t1

	/* Map one big (512Mb) page for ITLB */
	movi	MMUIR_FIRST, r21
	movi	MMUIR_TEXT_L, r22	/* PTEL first */
	putcfg	r21, 1, r22		/* Set MMUIR[0].PTEL */
	movi	MMUIR_TEXT_H, r22	/* PTEH last */
	putcfg	r21, 0, r22		/* Set MMUIR[0].PTEH */
	
	/* Map one big CACHED (512Mb) page for DTLB */
	movi	MMUDR_FIRST, r21
	movi	MMUDR_CACHED_L, r22	/* PTEL first */
	putcfg	r21, 1, r22		/* Set MMUDR[0].PTEL */
	movi	MMUDR_CACHED_H, r22	/* PTEH last */
	putcfg	r21, 0, r22		/* Set MMUDR[0].PTEH */
	
	/*
	 * Set cache behaviours.
	 */
	/* ICache */
	movi	ICCR_BASE, r21
	movi	ICCR0_INIT_VAL, r22
	movi	ICCR1_INIT_VAL, r23
	putcfg	r21, ICCR_REG0, r22
	putcfg	r21, ICCR_REG1, r23

	/* OCache */
	movi	OCCR_BASE, r21
	movi	OCCR0_INIT_VAL, r22
	movi	OCCR1_INIT_VAL, r23
	putcfg	r21, OCCR_REG0, r22
	putcfg	r21, OCCR_REG1, r23


	/*
	 * Enable Caches and MMU. Do the first non-PIC jump.
         * Now head.S global variables, constants and externs
	 * can be used.
	 */
	getcon	SR, r21
	movi	SR_ENABLE_MMU, r22
	or	r21, r22, r21
	putcon	r21, SSR
	_loada	hyperspace, r22
	ori	r22, 1, r22	    /* Make it SHmedia, not required but..*/
	putcon	r22, SPC
	synco
	rte			    /* And now go into the hyperspace ... */
hyperspace:			    /* ... that's the next instruction !  */

	/*
	 * Set CPU to a consistent state.
	 * r31 = FPU support flag
	 * t0/t7 in use. Others give a chance to loop somewhere safe
	 */
	_loada	start_kernel, r32
	ori	r32, 1, r32

	ptabs	r32, t0		    /* r32 = _start_kernel address        */
	_ptaru	hopeless, t1
	_ptaru	hopeless, t2
	_ptaru	hopeless, t3
	_ptaru	hopeless, t4
	_ptaru	hopeless, t5
	_ptaru	hopeless, t6
	_ptar	hopeless, t7
	gettr	t1, r28			/* r28 = hopeless address */

	/* Set initial stack pointer */
	_loada	init_task_union, SP
	putcon	SP, KCR0		/* Set current to init_task */
	movi	THREAD_SIZE, r22	/* Point to the end */
	add	SP, r22, SP

	/*
	 * Initialize FPU.
	 * Keep FPU flag in r31. After this block:	
	 * r31 = FPU flag
	 */
	addi	GVDT, fpu_in_use - variables, r31	/* Temporary */

#ifndef CONFIG_NOFPU_SUPPORT
	getcon	SR, r21
	movi	SR_ENABLE_FPU, r22
	and	r21, r22, r22
	putcon	r22, SR			/* Try to enable */
	getcon	SR, r22
	xor	r21, r22, r21
	shlri	r21, 15, r21		/* Supposedly 0/1 */
	st.q	r31, 0 , r21		/* Set fpu_in_use */
#else
	movi	0, r21
	st.q	r31, 0 , r21		/* Set fpu_in_use */
#endif
	or	r21, ZERO, r31		/* Set FPU flag at last */

	/*
	 * Clear bss
	 */
	_ptar	clear_quad, t1
	_loada	__bss_start, r22
	_loada	_end, r23
clear_quad:
	st.q	r22, 0, ZERO 
	addi	r22, 8, r22
	bne	r22, r23, t1		/* Both quad aligned, see vmlinux.lds.S */
	_ptaru	hopeless, t1

	/* Say bye to head.S but be prepared to wrongly get back ... */
	blink	t0, LINK

	/* If we ever get back here through LINK/t1-t7 */
	_ptaru	hopeless, t7

hopeless:
	/*
	 * Something's badly wrong here. Loop endlessly,
         * there's nothing more we can do about it.
	 *
	 * Note on hopeless: it can be jumped into invariably
	 * before or after jumping into hyperspace. The only
	 * requirement is to be PIC called (PTA) before and
	 * any way (PTA/PTABS) after. According to Virtual
	 * to Physical mapping a simulator/emulator can easily
	 * tell where we came here from just looking at hopeless
	 * (PC) address.
	 *
	 * For debugging purposes:
	 * (r28) hopeless/loop address
	 * (r29) Original SR
	 * (r30) CPU type/Platform endianness
	 * (r31) FPU Support
	 * (r32) _start_kernel address
	 */
	blink	t7, ZERO


	.balign L1_CACHE_BYTES
constants:
dummyc:	.quad 0

