/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998, 1999, 2000 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#include <asm/asm.h>
#include <asm/offset.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>

#define EX(insn,reg,addr,handler)			\
9:	insn	reg, addr;				\
	.section __ex_table,"a"; 			\
	PTR	9b, handler; 				\
	.previous

#define F_FILL64(dst, offset, val, fixup)		\
	EX(sd, val, (offset + 0x00)(dst), fixup);	\
	EX(sd, val, (offset + 0x08)(dst), fixup);	\
	EX(sd, val, (offset + 0x10)(dst), fixup);	\
	EX(sd, val, (offset + 0x18)(dst), fixup);	\
	EX(sd, val, (offset + 0x20)(dst), fixup);	\
	EX(sd, val, (offset + 0x28)(dst), fixup);	\
	EX(sd, val, (offset + 0x30)(dst), fixup);	\
	EX(sd, val, (offset + 0x38)(dst), fixup)

/*
 * memset(void *s, int c, size_t n)
 *
 * a0: start of area to clear
 * a1: char to fill with
 * a2: size of area to clear
 */
	.set	noreorder
	.align	5
LEAF(memset)
	beqz	a1, 1f
	 move	v0, a0				/* result */

	andi	a1, 0xff			/* spread fillword */
	dsll	t1, a1, 8
	or	a1, t1
	dsll	t1, a1, 16
	or	a1, t1
	dsll	t1, a1, 32
	or	a1, t1

1:

FEXPORT(__bzero)
	sltiu	t0, a2, 8			/* very small region? */
	bnez	t0, small_memset
	 andi	t0, a0, 7			/* aligned? */

	beqz	t0, 1f
	 dsubu	t0, 8				/* alignment in bytes */

#ifdef __MIPSEB__
	EX(sdl, a1, (a0), first_fixup)		/* make dword aligned */
#endif
#ifdef __MIPSEL__
	EX(sdr, a1, (a0), first_fixup)		/* make dword aligned */
#endif
	dsubu	a0, t0				/* dword align ptr */
	daddu	a2, t0				/* correct size */

1:	ori	t1, a2, 0x3f			/* # of full blocks */
	xori	t1, 0x3f
	beqz	t1, memset_partial		/* no block to fill */
	 andi	t0, a2, 0x38

	daddu	t1, a0				/* end address */
	.set	reorder
1:	daddiu	a0, 64
	F_FILL64(a0, -64, a1, fwd_fixup)
	bne	t1, a0, 1b
	.set	noreorder

memset_partial:
	PTR_LA	t1, 2f				/* where to start */
	.set	noat
	dsrl	AT, t0, 1
	dsubu	t1, AT
	.set	noat
	jr	t1
	 daddu	a0, t0				/* dest ptr */

	.set	push
	.set	noreorder
	.set	nomacro
	F_FILL64(a0, -64, a1, partial_fixup)	/* ... but first do dwds ... */
2:	.set	pop
	andi	a2, 7				/* 0 <= n <= 7 to go */

	beqz	a2, 1f
	 daddu	a0, a2				/* What's left */
#ifdef __MIPSEB__
	EX(sdr, a1, -1(a0), last_fixup)
#endif
#ifdef __MIPSEL__
	EX(sdl, a1, -1(a0), last_fixup)
#endif
1:	jr	ra
	 move	a2, zero

small_memset:
	beqz	a2, 2f
	 daddu	t1, a0, a2

1:	daddiu	a0, 1				/* fill bytewise */
	bne	t1, a0, 1b
	 sb	a1, -1(a0)

2:	jr	ra				/* done */
	 move	a2, zero
	END(memset)

first_fixup:
	jr	ra
	 nop

fwd_fixup:
	ld	t0, THREAD_BUADDR($28)
	andi	a2, 0x3f
	daddu	a2, t1
	jr	ra
	 dsubu	a2, t0

partial_fixup:
	ld	t0, THREAD_BUADDR($28)
	andi	a2, 7
	daddu	a2, t1
	jr	ra
	 dsubu	a2, t0

last_fixup:
	jr	ra
	 andi	v1, a2, 7
