/*
 * Initial implementation:
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 *
 * As long as the above copyright statement and this notice remain
 * unchanged, you can do what ever you want with this file. 
 *
 * $FreeBSD$
 */

#define	LOCORE

#include <machine/asi.h>
#include <machine/asm.h>
#include <machine/pstate.h>
#include <machine/smp.h>
#include <machine/upa.h>

#define	PAGE_SIZE	8192
#define	PAGE_SHIFT	13

#define	SPOFF		2047
#define	STACK_SIZE	(2 * PAGE_SIZE)

ENTRY(_start)
	/* limit interrupts */
	wrpr	%g0, 13, %pil

	/*
	 * PSTATE: privileged, interrupts enabled, floating point
	 * unit enabled
	 */
	wrpr	%g0, PSTATE_PRIV|PSTATE_IE|PSTATE_PEF, %pstate
	wr	%g0, 0x4, %fprs

	setx	stack + STACK_SIZE - SPOFF - CCFSZ, %l7, %l6
	mov	%l6, %sp
	call	main
	 mov	%o4, %o0
	sir

/*
 * %o0 input VA constant
 * %o1 current iTLB offset
 * %o2 current iTLB TTE tag
 */
ENTRY(itlb_va_to_pa)
	clr	%o1
0:	ldxa	[%o1] ASI_ITLB_TAG_READ_REG, %o2
	cmp	%o2, %o0
	bne,a	%xcc, 1f
	 nop
	/* return PA of matching entry */
	ldxa	[%o1] ASI_ITLB_DATA_ACCESS_REG, %o0
	sllx	%o0, 23, %o0
	srlx	%o0, PAGE_SHIFT+23, %o0
	sllx	%o0, PAGE_SHIFT, %o0
	retl
	 mov	%o0, %o1
1:	cmp	%o1, 63<<3
	blu	%xcc, 0b
	 add	%o1, 8, %o1
	clr	%o0
	retl
	 not	%o0

ENTRY(dtlb_va_to_pa)
	clr	%o1
0:	ldxa	[%o1] ASI_DTLB_TAG_READ_REG, %o2
	cmp	%o2, %o0
	bne,a	%xcc, 1f
	 nop
	/* return PA of matching entry */
	ldxa	[%o1] ASI_DTLB_DATA_ACCESS_REG, %o0
	sllx	%o0, 23, %o0
	srlx	%o0, PAGE_SHIFT+23, %o0
	sllx	%o0, PAGE_SHIFT, %o0
	retl
	 mov	%o0, %o1
1:	cmp	%o1, 63<<3
	blu	%xcc, 0b
	 add	%o1, 8, %o1
	clr	%o0
	retl
	 not	%o0

/*
 * %o0 = slot number
 * %o1 = vpn
 * %o2 = tte data
 */
ENTRY(itlb_enter)
	rdpr	%pstate, %o4
	wrpr	%o4, PSTATE_IE, %pstate
	sllx	%o0, 3, %o0
	sllx	%o1, PAGE_SHIFT, %o1
	mov	AA_IMMU_TAR, %o3
	stxa	%o1, [%o3] ASI_IMMU
	stxa	%o2, [%o0] ASI_ITLB_DATA_ACCESS_REG
	membar	#Sync
	retl
	 wrpr	%o4, 0, %pstate

ENTRY(dtlb_enter)
	rdpr	%pstate, %o4
	wrpr	%o4, PSTATE_IE, %pstate
	sllx	%o0, 3, %o0
	sllx	%o1, PAGE_SHIFT, %o1
	mov	AA_DMMU_TAR, %o3
	stxa	%o1, [%o3] ASI_DMMU
	stxa	%o2, [%o0] ASI_DTLB_DATA_ACCESS_REG
	membar	#Sync
	retl
	 wrpr	%o4, 0, %pstate

	.comm	stack, STACK_SIZE, 32
	.comm	smp_stack, STACK_SIZE, 32
