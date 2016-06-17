/* -*- mode: fundamental -*- */

/*
 * SN2 specific ivt(s)
 *
 * archi/ia64/sn/kernel/setup.c will dynamically replace code in the
 * ivt with code from here
 *
 * Please note: We need to be sure that any changes in ivt.S are also
 * reflected here (for example, if the ia64_ivt_page_fault register
 * usage changes) and vice-versa.
 *
 * Copyright (c) 1998-2003 Hewlett-Packard Company
 *	Stephane Eranian <eranian@hpl.hp.com>
 *	David Mosberger <davidm@hpl.hp.com>
 *
 * Copyright (c) 2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 */

#include <linux/config.h>

#include <asm/asmmacro.h>
#include <asm/break.h>
#include <asm/kregs.h>
#include <asm/offsets.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/system.h>
#include <asm/unistd.h>


/* If someone has a *really* good reason to disable the VHPT for SN2 I'll fix this. --cw */
#ifdef CONFIG_DISABLE_VHPT
#error SN2 requires VHPT be enabled
#endif

	.section ".text.init", "ax"

/*
 * SN2 specific region 6/7 dtlb miss handler.
 *
 * On SN2 some granule-0 addresses (and therefore any aliases) are
 * actually used uncachable.  We load small TC entries to ensure there
 * is no-overlap between such regions (which could cause a Cache-Check
 * MCA).
 */
#define SN2_GMASK	(((1 << (36-IA64_GRANULE_SHIFT)) - 1) << IA64_GRANULE_SHIFT)
	.global sn2_alt_dtlb_miss
ENTRY(sn2_alt_dtlb_miss)
	mov r16=cr.ifa					// get address that caused the TLB miss
	movl r17=PAGE_KERNEL				// kernel protection bits (RWX)
	mov r20=cr.isr					// need to check for SP and NA status
	movl r19=(((1<<IA64_MAX_PHYS_BITS)-1) & ~0xfff)	// suitable mask
	mov r21=cr.ipsr					// get ipsr incase we need to poke the ED bit
	mov r31=pr					// save pr's
	mov r25=_PAGE_SIZE_4K<<2				// granule-0 requires we use smaller pages
	;;
	movl r27=SN2_GMASK				// Mask suitable to fine granule-0 (aliased) addresses
	tbit.nz p9,p0=r20,IA64_ISR_NA_BIT			// is non-access bit on?
	and r22=IA64_ISR_CODE_MASK,r20			// get the isr.code field
	extr.u r23=r21,IA64_PSR_CPL0_BIT,2			// extract psr.cpl
	shr.u r26=r16,61					// region number
	;;
	tbit.nz p6,p7=r20,IA64_ISR_SP_BIT			// is speculation bit on?
	and r19=r19,r16					// clear ED, reserved bits, and PTE control bits
	cmp.eq p10,p11=7,r26				// p11 <- region 7 (else p10 <- !0 => region 6)
	and r24=r27,r16					// mask away all but region bits
	;;
	cmp.ne.andcm p10,p0=r0,r24			// p10 <- region-6 AND granule-0

/* arch/ia64/sn/kernel/setup.c patches this code, you should check there if you need to mess about with this */
	.global sn2_alt_dtlb_miss_patch1
sn2_alt_dtlb_miss_patch1:
	{ .mib
(p9)	cmp.eq.or.andcm p6,p7=IA64_ISR_CODE_LFETCH,r22	// check isr.code field
	cmp.ne p8,p0=r0,r23				// p8 <- cpl == 0?
(p8)	br.cond.spnt.many ia64_ivt_page_fault		// NB: gets patched
	}

	;;
(p10)	mov cr.itir=r25					// use smaller pagesize for tc
	dep r21=-1,r21,IA64_PSR_ED_BIT,1			// r21 is psr with ED enabled
	or r19=r19,r17					// insert PTE control bits into r19
	;;
(p6)	mov cr.ipsr=r21					// p6 (speculation): set ed (else we can get stuck)
(p11)	dep r19=-1,r19,4,1				// set bit 4 (uncached) if the access was to region 6
	;;
(p7)	itc.d r19					// insert the TLB entry
	mov pr=r31,-1					// restore pr's
	rfi
END(alt_dtlb_miss)
	.align 8
	.global sn2_alt_dtlb_miss_end
sn2_alt_dtlb_miss_end:
