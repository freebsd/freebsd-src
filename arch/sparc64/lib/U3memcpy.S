/* $Id: U3memcpy.S,v 1.2 2000/11/01 09:29:19 davem Exp $
 * U3memcpy.S: UltraSparc-III optimized memcpy.
 *
 * Copyright (C) 1999, 2000 David S. Miller (davem@redhat.com)
 */

#ifdef __KERNEL__
#include <asm/visasm.h>
#include <asm/asi.h>
#include <asm/dcu.h>
#include <asm/spitfire.h>
#undef SMALL_COPY_USES_FPU
#else
#define ASI_BLK_P 0xf0
#define FPRS_FEF  0x04
#define VISEntryHalf rd %fprs, %o5; wr %g0, FPRS_FEF, %fprs
#define VISExitHalf and %o5, FPRS_FEF, %o5; wr %o5, 0x0, %fprs
#define SMALL_COPY_USES_FPU
#endif

	/* Special/non-trivial issues of this code:
	 *
	 * 1) %o5 is preserved from VISEntryHalf to VISExitHalf
	 * 2) Only low 32 FPU registers are used so that only the
	 *    lower half of the FPU register set is dirtied by this
	 *    code.  This is especially important in the kernel.
	 * 3) This code never prefetches cachelines past the end
	 *    of the source buffer.
	 */

	.text
	.align	32

	/* The cheetah's flexible spine, oversized liver, enlarged heart,
	 * slender muscular body, and claws make it the swiftest hunter
	 * in Africa and the fastest animal on land.  Can reach speeds
	 * of up to 2.4GB per second.
	 */

	.globl		U3memcpy
U3memcpy: /* %o0=dst, %o1=src, %o2=len */
#ifndef __KERNEL__
	/* Save away original 'dst' for memcpy return value. */
	mov		%o0, %g3			! A0	Group
#endif
	/* Anything to copy at all? */
	cmp		%o2, 0				! A1
	ble,pn		%icc, U3memcpy_short_ret	! BR

	/* Extremely small copy? */
	 cmp		%o2, 31				! A0	Group
	ble,pn		%icc, U3memcpy_short		! BR

	/* Large enough to use unrolled prefetch loops? */
	 cmp		%o2, 0x100			! A1
	bge,a,pt	%icc, U3memcpy_enter		! BR	Group
	 andcc		%o0, 0x3f, %g2			! A0

	ba,pt		%xcc, U3memcpy_toosmall		! BR	Group
	 andcc		%o0, 0x7, %g2			! A0

	.align		32
U3memcpy_short:
	/* Copy %o2 bytes from src to dst, one byte at a time. */
	ldub		[%o1 + 0x00], %o3		! MS	Group
	add		%o1, 0x1, %o1			! A0
	add		%o0, 0x1, %o0			! A1
	subcc		%o2, 1, %o2			! A0	Group

	bg,pt		%icc, U3memcpy_short		! BR
	 stb		%o3, [%o0 + -1]			! MS	Group (1-cycle stall)

U3memcpy_short_ret:
#ifdef __KERNEL__
	retl						! BR	Group (0-4 cycle stall)
	 clr		%o0				! A0
#else
	retl						! BR	Group (0-4 cycle stall)
	 mov		%g3, %o0			! A0
#endif

	/* Here len >= (6 * 64) and condition codes reflect execution
	 * of "andcc %o0, 0x7, %g2", done by caller.
	 */
	.align		64
U3memcpy_enter:
	/* Is 'dst' already aligned on an 64-byte boundary? */
	be,pt		%xcc, 2f			! BR

	/* Compute abs((dst & 0x3f) - 0x40) into %g2.  This is the number
	 * of bytes to copy to make 'dst' 64-byte aligned.  We pre-
	 * subtract this from 'len'.
	 */
	 sub		%g2, 0x40, %g2			! A0	Group
	sub		%g0, %g2, %g2			! A0	Group
	sub		%o2, %g2, %o2			! A0	Group

	/* Copy %g2 bytes from src to dst, one byte at a time. */
1:	ldub		[%o1 + 0x00], %o3		! MS	(Group)
	add		%o1, 0x1, %o1			! A1
	add		%o0, 0x1, %o0			! A0	Group
	subcc		%g2, 0x1, %g2			! A1

	bg,pt		%icc, 1b			! BR	Group
	 stb		%o3, [%o0 + -1]			! MS	Group

2:	VISEntryHalf					! MS+MS
	and		%o1, 0x7, %g1			! A1
	ba,pt		%xcc, U3memcpy_begin		! BR
	 alignaddr	%o1, %g0, %o1			! MS	      (Break-after)

	.align		64
U3memcpy_begin:
#ifdef __KERNEL__
	.globl		U3memcpy_nop_1_6
U3memcpy_nop_1_6:
	ldxa		[%g0] ASI_DCU_CONTROL_REG, %g3
	sethi		%uhi(DCU_PE), %o3
	sllx		%o3, 32, %o3
	or		%g3, %o3, %o3
	stxa		%o3, [%g0] ASI_DCU_CONTROL_REG	! Enable P-cache
	membar		#Sync
#endif
	prefetch	[%o1 + 0x000], #one_read	! MS	Group1
	prefetch	[%o1 + 0x040], #one_read	! MS	Group2
	andn		%o2, (0x40 - 1), %o4		! A0
	prefetch	[%o1 + 0x080], #one_read	! MS	Group3
	cmp		%o4, 0x140			! A0
	prefetch	[%o1 + 0x0c0], #one_read	! MS	Group4
	ldd		[%o1 + 0x000], %f0		! MS	Group5 (%f0 results at G8)
	bge,a,pt	%icc, 1f			! BR

	 prefetch	[%o1 + 0x100], #one_read	! MS	Group6
1:	ldd		[%o1 + 0x008], %f2		! AX	       (%f2 results at G9)
	cmp		%o4, 0x180			! A1
	bge,a,pt	%icc, 1f			! BR
	 prefetch	[%o1 + 0x140], #one_read	! MS	Group7
1:	ldd		[%o1 + 0x010], %f4		! AX	       (%f4 results at G10)
	cmp		%o4, 0x1c0			! A1
	bge,a,pt	%icc, 1f			! BR

	 prefetch	[%o1 + 0x180], #one_read	! MS	Group8
1:	faligndata	%f0, %f2, %f16			! FGA	Group9 (%f16 at G12)
	ldd		[%o1 + 0x018], %f6		! AX	       (%f6 results at G12)
	faligndata	%f2, %f4, %f18			! FGA	Group10 (%f18 results at G13)
	ldd		[%o1 + 0x020], %f8		! MS	        (%f8 results at G13)
	faligndata	%f4, %f6, %f20			! FGA	Group12	(1-cycle stall,%f20 at G15)
	ldd		[%o1 + 0x028], %f10		! MS		(%f10 results at G15)
	faligndata	%f6, %f8, %f22			! FGA	Group13 (%f22 results at G16)

	ldd		[%o1 + 0x030], %f12		! MS		(%f12 results at G16)
	faligndata	%f8, %f10, %f24			! FGA	Group15 (1-cycle stall,%f24 at G18)
	ldd		[%o1 + 0x038], %f14		! MS		(%f14 results at G18)
	faligndata	%f10, %f12, %f26		! FGA	Group16 (%f26 results at G19)
	ldd		[%o1 + 0x040], %f0		! MS		(%f0 results at G19)

	/* We only use the first loop if len > (7 * 64). */
	subcc		%o4, 0x1c0, %o4			! A0	Group17
	bg,pt		%icc, U3memcpy_loop1		! BR
	 add		%o1, 0x40, %o1			! A1

	add		%o4, 0x140, %o4			! A0	Group18
	ba,pt		%xcc, U3memcpy_loop2		! BR
	 srl		%o4, 6, %o3			! A0	Group19
	nop
	nop
	nop
	nop
	nop

	nop
	nop

	/* This loop performs the copy and queues new prefetches.
	 * We drop into the second loop when len <= (5 * 64).  Note
	 * that this (5 * 64) factor has been subtracted from len
	 * already.
	 */
U3memcpy_loop1:
	ldd		[%o1 + 0x008], %f2		! MS	Group2	(%f2 results at G5)
	faligndata	%f12, %f14, %f28		! FGA		(%f28 results at G5)
	ldd		[%o1 + 0x010], %f4		! MS	Group3	(%f4 results at G6)
	faligndata	%f14, %f0, %f30			! FGA	Group4	(1-cycle stall, %f30 at G7)
	stda		%f16, [%o0] ASI_BLK_P		! MS
	ldd		[%o1 + 0x018], %f6		! AX		(%f6 results at G7)

	faligndata	%f0, %f2, %f16			! FGA	Group12 (7-cycle stall)
	ldd		[%o1 + 0x020], %f8		! MS	        (%f8 results at G15)
	faligndata	%f2, %f4, %f18			! FGA	Group13	(%f18 results at G16)
	ldd		[%o1 + 0x028], %f10		! MS		(%f10 results at G16)
	faligndata	%f4, %f6, %f20			! FGA	Group14	(%f20 results at G17)
	ldd		[%o1 + 0x030], %f12		! MS		(%f12 results at G17)
	faligndata	%f6, %f8, %f22			! FGA	Group15	(%f22 results at G18)
	ldd		[%o1 + 0x038], %f14		! MS		(%f14 results at G18)

	faligndata	%f8, %f10, %f24			! FGA	Group16	(%f24 results at G19)
	ldd		[%o1 + 0x040], %f0		! AX		(%f0 results at G19)
	prefetch	[%o1 + 0x180], #one_read	! MS
	faligndata	%f10, %f12, %f26		! FGA	Group17	(%f26 results at G20)
	subcc		%o4, 0x40, %o4			! A0
	add		%o1, 0x40, %o1			! A1
	bg,pt		%xcc, U3memcpy_loop1		! BR
	 add		%o0, 0x40, %o0			! A0	Group18

U3memcpy_loop2_enter:
	mov		5, %o3				! A1

	/* This loop performs on the copy, no new prefetches are
	 * queued.  We do things this way so that we do not perform
	 * any spurious prefetches past the end of the src buffer.
	 */
U3memcpy_loop2:
	ldd		[%o1 + 0x008], %f2		! MS
	faligndata	%f12, %f14, %f28		! FGA	Group2
	ldd		[%o1 + 0x010], %f4		! MS
	faligndata	%f14, %f0, %f30			! FGA	Group4	(1-cycle stall)
	stda		%f16, [%o0] ASI_BLK_P		! MS
	ldd		[%o1 + 0x018], %f6		! AX
	faligndata	%f0, %f2, %f16			! FGA	Group12 (7-cycle stall)

	ldd		[%o1 + 0x020], %f8		! MS
	faligndata	%f2, %f4, %f18			! FGA	Group13
	ldd		[%o1 + 0x028], %f10		! MS
	faligndata	%f4, %f6, %f20			! FGA	Group14
	ldd		[%o1 + 0x030], %f12		! MS
	faligndata	%f6, %f8, %f22			! FGA	Group15
	ldd		[%o1 + 0x038], %f14		! MS
	faligndata	%f8, %f10, %f24			! FGA	Group16

	ldd		[%o1 + 0x040], %f0		! AX
	faligndata	%f10, %f12, %f26		! FGA	Group17
	subcc		%o3, 0x01, %o3			! A0
	add		%o1, 0x40, %o1			! A1
	bg,pt		%xcc, U3memcpy_loop2		! BR
	 add		%o0, 0x40, %o0			! A0	Group18

	/* Finally we copy the last full 64-byte block. */
U3memcpy_loopfini:
	ldd		[%o1 + 0x008], %f2		! MS
	faligndata	%f12, %f14, %f28		! FGA
	ldd		[%o1 + 0x010], %f4		! MS	Group19
	faligndata	%f14, %f0, %f30			! FGA
	stda		%f16, [%o0] ASI_BLK_P		! MS	Group20
	ldd		[%o1 + 0x018], %f6		! AX
	faligndata	%f0, %f2, %f16			! FGA	Group11 (7-cycle stall)
	ldd		[%o1 + 0x020], %f8		! MS
	faligndata	%f2, %f4, %f18			! FGA	Group12
	ldd		[%o1 + 0x028], %f10		! MS
	faligndata	%f4, %f6, %f20			! FGA	Group13
	ldd		[%o1 + 0x030], %f12		! MS
	faligndata	%f6, %f8, %f22			! FGA	Group14
	ldd		[%o1 + 0x038], %f14		! MS
	faligndata	%f8, %f10, %f24			! FGA	Group15
	cmp		%g1, 0				! A0
	be,pt		%icc, 1f			! BR
	 add		%o0, 0x40, %o0			! A1
	ldd		[%o1 + 0x040], %f0		! MS
1:	faligndata	%f10, %f12, %f26		! FGA	Group16
	faligndata	%f12, %f14, %f28		! FGA	Group17
	faligndata	%f14, %f0, %f30			! FGA	Group18
	stda		%f16, [%o0] ASI_BLK_P		! MS
	add		%o0, 0x40, %o0			! A0
	add		%o1, 0x40, %o1			! A1
#ifdef __KERNEL__
	.globl		U3memcpy_nop_2_3
U3memcpy_nop_2_3:
	mov		PRIMARY_CONTEXT, %o3
	stxa		%g0, [%o3] ASI_DMMU		! Flush P-cache
	stxa		%g3, [%g0] ASI_DCU_CONTROL_REG	! Disable P-cache
#endif
	membar		#Sync				! MS	Group26 (7-cycle stall)

	/* Now we copy the (len modulo 64) bytes at the end.
	 * Note how we borrow the %f0 loaded above.
	 *
	 * Also notice how this code is careful not to perform a
	 * load past the end of the src buffer just like similar
	 * code found in U3memcpy_toosmall processing.
	 */
U3memcpy_loopend:
	and		%o2, 0x3f, %o2			! A0	Group
	andcc		%o2, 0x38, %g2			! A0	Group
	be,pn		%icc, U3memcpy_endcruft		! BR
	 subcc		%g2, 0x8, %g2			! A1
	be,pn		%icc, U3memcpy_endcruft		! BR	Group
	 cmp		%g1, 0				! A0

	be,a,pt		%icc, 1f			! BR	Group
	 ldd		[%o1 + 0x00], %f0		! MS

1:	ldd		[%o1 + 0x08], %f2		! MS	Group
	add		%o1, 0x8, %o1			! A0
	sub		%o2, 0x8, %o2			! A1
	subcc		%g2, 0x8, %g2			! A0	Group
	faligndata	%f0, %f2, %f8			! FGA	Group
	std		%f8, [%o0 + 0x00]		! MS	(XXX does it stall here? XXX)
	be,pn		%icc, U3memcpy_endcruft		! BR
	 add		%o0, 0x8, %o0			! A0
	ldd		[%o1 + 0x08], %f0		! MS	Group
	add		%o1, 0x8, %o1			! A0
	sub		%o2, 0x8, %o2			! A1
	subcc		%g2, 0x8, %g2			! A0	Group
	faligndata	%f2, %f0, %f8			! FGA
	std		%f8, [%o0 + 0x00]		! MS	(XXX does it stall here? XXX)
	bne,pn		%icc, 1b			! BR
	 add		%o0, 0x8, %o0			! A0	Group

	/* If anything is left, we copy it one byte at a time.
	 * Note that %g1 is (src & 0x3) saved above before the
	 * alignaddr was performed.
	 */
U3memcpy_endcruft:
	cmp		%o2, 0
	add		%o1, %g1, %o1
	VISExitHalf
	be,pn		%icc, U3memcpy_short_ret
	 nop
	ba,a,pt		%xcc, U3memcpy_short

	/* If we get here, then 32 <= len < (6 * 64) */
U3memcpy_toosmall:

#ifdef SMALL_COPY_USES_FPU

	/* Is 'dst' already aligned on an 8-byte boundary? */
	be,pt		%xcc, 2f			! BR	Group

	/* Compute abs((dst & 7) - 8) into %g2.  This is the number
	 * of bytes to copy to make 'dst' 8-byte aligned.  We pre-
	 * subtract this from 'len'.
	 */
	 sub		%g2, 0x8, %g2			! A0
	sub		%g0, %g2, %g2			! A0	Group (reg-dep)
	sub		%o2, %g2, %o2			! A0	Group (reg-dep)

	/* Copy %g2 bytes from src to dst, one byte at a time. */
1:	ldub		[%o1 + 0x00], %o3		! MS	(Group) (%o3 in 3 cycles)
	add		%o1, 0x1, %o1			! A1
	add		%o0, 0x1, %o0			! A0	Group
	subcc		%g2, 0x1, %g2			! A1

	bg,pt		%icc, 1b			! BR	Group
	 stb		%o3, [%o0 + -1]			! MS	Group

2:	VISEntryHalf					! MS+MS

	/* Compute (len - (len % 8)) into %g2.  This is guarenteed
	 * to be nonzero.
	 */
	andn		%o2, 0x7, %g2			! A0	Group

	/* You may read this and believe that it allows reading
	 * one 8-byte longword past the end of src.  It actually
	 * does not, as %g2 is subtracted as loads are done from
	 * src, so we always stop before running off the end.
	 * Also, we are guarenteed to have at least 0x10 bytes
	 * to move here.
	 */
	sub		%g2, 0x8, %g2			! A0	Group (reg-dep)
	alignaddr	%o1, %g0, %g1			! MS	      (Break-after)
	ldd		[%g1 + 0x00], %f0		! MS	Group (1-cycle stall)
	add		%g1, 0x8, %g1			! A0

1:	ldd		[%g1 + 0x00], %f2		! MS	Group
	add		%g1, 0x8, %g1			! A0
	sub		%o2, 0x8, %o2			! A1
	subcc		%g2, 0x8, %g2			! A0	Group

	faligndata	%f0, %f2, %f8			! FGA	Group (1-cycle stall)
	std		%f8, [%o0 + 0x00]		! MS	Group (2-cycle stall)
	add		%o1, 0x8, %o1			! A0
	be,pn		%icc, 2f			! BR

	 add		%o0, 0x8, %o0			! A1
	ldd		[%g1 + 0x00], %f0		! MS	Group
	add		%g1, 0x8, %g1			! A0
	sub		%o2, 0x8, %o2			! A1

	subcc		%g2, 0x8, %g2			! A0	Group
	faligndata	%f2, %f0, %f8			! FGA	Group (1-cycle stall)
	std		%f8, [%o0 + 0x00]		! MS	Group (2-cycle stall)
	add		%o1, 0x8, %o1			! A0

	bne,pn		%icc, 1b			! BR
	 add		%o0, 0x8, %o0			! A1

	/* Nothing left to copy? */
2:	cmp		%o2, 0				! A0	Group
	VISExitHalf					! A0+MS
	be,pn		%icc, U3memcpy_short_ret	! BR	Group
	 nop						! A0
	ba,a,pt		%xcc, U3memcpy_short		! BR	Group

#else /* !(SMALL_COPY_USES_FPU) */

	xor		%o1, %o0, %g2
	andcc		%g2, 0x7, %g0
	bne,pn		%icc, U3memcpy_short
	 andcc		%o1, 0x7, %g2

	be,pt		%xcc, 2f
	 sub		%g2, 0x8, %g2
	sub		%g0, %g2, %g2
	sub		%o2, %g2, %o2

1:	ldub		[%o1 + 0x00], %o3
	add		%o1, 0x1, %o1
	add		%o0, 0x1, %o0
	subcc		%g2, 0x1, %g2
	bg,pt		%icc, 1b
	 stb		%o3, [%o0 + -1]

2:	andn		%o2, 0x7, %g2
	sub		%o2, %g2, %o2

3:	ldx		[%o1 + 0x00], %o3
	add		%o1, 0x8, %o1
	add		%o0, 0x8, %o0
	subcc		%g2, 0x8, %g2
	bg,pt		%icc, 3b
	 stx		%o3, [%o0 + -8]

	cmp		%o2, 0
	bne,pn		%icc, U3memcpy_short
	 nop
	ba,a,pt		%xcc, U3memcpy_short_ret

#endif /* !(SMALL_COPY_USES_FPU) */
