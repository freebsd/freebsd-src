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
#include <machine/asi.h>
#include <machine/pstate.h>
#include <machine/param.h>

#define	BIAS		2047
#define	CC64FSZ		192
#define	TLB_TAG_ACCESS	0x30

	.text
	.globl _start
_start:
	setx	ofw_entry, %l7, %l0
	stx	%o4, [%l0]

	/* limit interrupts */
	wrpr	%g0, 13, %pil

	/*
	 * PSTATE: privileged, interrupts enabled, floating point
	 * unit enabled
	 */
	wrpr	%g0, PSTATE_PRIV|PSTATE_IE|PSTATE_PEF, %pstate
	wr	%o0, 0x4, %fprs

	setx	stack, %l7, %sp
	call	main
	 nop

/*
 * %o0 kernel entry (VA)
 * %o1 bootinfo structure pointer (VA)
 *
 * XXX Does the FreeBSD kernel expect the bootinfo pointer
 *     in %o0 or in %o1?
 */
.globl jmpkern
jmpkern:
	setx	ofw_entry, %l7, %o2
	jmp	%o0
	 ldx	[%o2], %o2

/*
 * %o0 input VA constant
 * %o1 current iTLB offset
 * %o2 current iTLB TTE tag
 */
.globl itlb_va_to_pa
itlb_va_to_pa:
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

.globl dtlb_va_to_pa
dtlb_va_to_pa:
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
 * %o1 = pa
 * %o2 = va
 * %o3 = flags
 */
.globl itlb_enter
itlb_enter:
	sllx	%o0, 3, %o0
	or	%o1, %o3, %o1
	mov	TLB_TAG_ACCESS, %o3
	stxa	%o2, [%o3] ASI_IMMU
	membar	#Sync
	stxa	%o1, [%o0] ASI_ITLB_DATA_ACCESS_REG
	retl
	 nop

.globl dtlb_enter
dtlb_enter:
	sllx	%o0, 3, %o0
	or	%o1, %o3, %o1
	mov	TLB_TAG_ACCESS, %o3
	stxa	%o2, [%o3] ASI_DMMU
	membar	#Sync
	stxa	%o1, [%o0] ASI_DTLB_DATA_ACCESS_REG
	retl
	 nop

.globl ofw_gate
ofw_gate:
	save	%sp, -CC64FSZ, %sp
	setx	ofw_entry, %i3, %i4
	ldx	[%i4], %i4

	mov	%g1, %l1
	mov	%g2, %l2
	mov	%g3, %l3
	mov	%g4, %l4
	mov	%g5, %l5
	mov	%g6, %l6
	mov	%g7, %l7

	rdpr	%pstate, %i3
	wrpr	%g0, PSTATE_PRIV, %pstate

	jmpl	%i4, %o7
	 mov	%i0, %o0
	mov	%o0, %i0

	wrpr	%i3, 0, %pstate

	mov	%l1, %g1
	mov	%l2, %g2
	mov	%l3, %g3
	mov	%l4, %g4
	mov	%l5, %g5
	mov	%l6, %g6
	mov	%l7, %g7

	ret
	 restore

	.data
	.align 8
ofw_entry:	.xword	0

	.align 32
	.space	0x4000
	.set	stack, _stack-BIAS
_stack:
	.space 0x10000
