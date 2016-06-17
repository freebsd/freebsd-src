/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1998 by Ralf Baechle
 */
#include <asm/asm.h>
#include <asm/offset.h>
#include <asm/regdef.h>

#define EX(insn,reg,addr,handler)			\
9:	insn	reg, addr;				\
	.section __ex_table,"a"; 			\
	PTR	9b, handler; 				\
	.previous

#define F_FILL64(dst, offset, val, fixup)		\
	EX(sw, val, (offset + 0x00)(dst), fixup);	\
	EX(sw, val, (offset + 0x04)(dst), fixup);	\
	EX(sw, val, (offset + 0x08)(dst), fixup);	\
	EX(sw, val, (offset + 0x0c)(dst), fixup);	\
	EX(sw, val, (offset + 0x10)(dst), fixup);	\
	EX(sw, val, (offset + 0x14)(dst), fixup);	\
	EX(sw, val, (offset + 0x18)(dst), fixup);	\
	EX(sw, val, (offset + 0x1c)(dst), fixup);	\
	EX(sw, val, (offset + 0x20)(dst), fixup);	\
	EX(sw, val, (offset + 0x24)(dst), fixup);	\
	EX(sw, val, (offset + 0x28)(dst), fixup);	\
	EX(sw, val, (offset + 0x2c)(dst), fixup);	\
	EX(sw, val, (offset + 0x30)(dst), fixup);	\
	EX(sw, val, (offset + 0x34)(dst), fixup);	\
	EX(sw, val, (offset + 0x38)(dst), fixup);	\
	EX(sw, val, (offset + 0x3c)(dst), fixup)

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
	sll	t1, a1, 8
	or	a1, t1
	sll	t1, a1, 16
	or	a1, t1
1:

EXPORT(__bzero)
	sltiu	t0, a2, 4			/* very small region? */
	bnez	t0, small_memset
	 andi	t0, a0, 3			/* aligned? */

	beqz	t0, 1f
	 subu	t0, 4				/* alignment in bytes */

#ifdef __MIPSEB__
	EX(swl, a1, (a0), first_fixup)		/* make word aligned */
#endif
#ifdef __MIPSEL__
	EX(swr, a1, (a0), first_fixup)		/* make word aligned */
#endif
	subu	a0, t0				/* word align ptr */
	addu	a2, t0				/* correct size */

1:	ori	t1, a2, 0x3f			/* # of full blocks */
	xori	t1, 0x3f
	beqz	t1, memset_partial		/* no block to fill */
	 andi	t0, a2, 0x3c

	addu	t1, a0				/* end address */
	.set	reorder
1:	addiu	a0, 64
	F_FILL64(a0, -64, a1, fwd_fixup)
	bne	t1, a0, 1b
	.set	noreorder

memset_partial:
	PTR_LA	t1, 2f				/* where to start */
	subu	t1, t0
	jr	t1
	 addu	a0, t0				/* dest ptr */

	.set	push
	.set	noreorder
	.set	nomacro
	F_FILL64(a0, -64, a1, partial_fixup)	/* ... but first do wrds ... */
2:	.set	pop
	andi	a2, 3				/* 0 <= n <= 3 to go */

	beqz	a2, 1f
	 addu	a0, a2				/* What's left */
#ifdef __MIPSEB__
	EX(swr, a1, -1(a0), last_fixup)
#endif
#ifdef __MIPSEL__
	EX(swl, a1, -1(a0), last_fixup)
#endif
1:	jr	ra
	 move	a2, zero

small_memset:
	beqz	a2, 2f
	 addu	t1, a0, a2

1:	addiu	a0, 1				/* fill bytewise */
	bne	t1, a0, 1b
	 sb	a1, -1(a0)

2:	jr	ra				/* done */
	 move	a2, zero
	END(memset)

first_fixup:
	jr	ra
	 nop

fwd_fixup:
	lw	t0, THREAD_BUADDR($28)
	andi	a2, 0x3f
	addu	a2, t1
	jr	ra
	 subu	a2, t0

partial_fixup:
	lw	t0, THREAD_BUADDR($28)
	andi	a2, 3
	addu	a2, t1
	jr	ra
	 subu	a2, t0

last_fixup:
	jr	ra
	 andi	v1, a2, 3
