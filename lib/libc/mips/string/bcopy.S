/*	$NetBSD: bcopy.S,v 1.2 2005/12/27 11:23:53 tsutsui Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 *	File:	mips_bcopy.s
 *	Author:	Chris Maeda
 *	Date:	June 1993
 *
 *	Fast copy routine.  Derived from aligned_block_copy.
 */


#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#if defined(LIBC_SCCS) && !defined(lint)
	ASMSTR("from: @(#)mips_bcopy.s	2.2 CMU 18/06/93")
	ASMSTR("$NetBSD: bcopy.S,v 1.2 2005/12/27 11:23:53 tsutsui Exp $")
#endif /* LIBC_SCCS and not lint */

#ifdef __ABICALLS__
	.abicalls
#endif

/*
 *	bcopy(caddr_t src, caddr_t dst, unsigned int len)
 *
 *	a0 	src address
 *	a1	dst address
 *	a2	length
 */

#if defined(MEMCOPY) || defined(MEMMOVE)
#ifdef MEMCOPY
#define	FUNCTION	memcpy
#else
#define FUNCTION	memmove
#endif
#define	SRCREG		a1
#define	DSTREG		a0
#else
#define	FUNCTION	bcopy
#define	SRCREG		a0
#define	DSTREG		a1
#endif

#define	SIZEREG		a2

LEAF(FUNCTION)
	.set	noat
	.set	noreorder

#if defined(MEMCOPY) || defined(MEMMOVE)
	/* set up return value, while we still can */
	move	v0,DSTREG
#endif
	/*
	 *	Make sure we can copy forwards.
	 */
	sltu	t0,SRCREG,DSTREG	# t0 == SRCREG < DSTREG
	bne	t0,zero,6f		# copy backwards

	/*
	 * 	There are four alignment cases (with frequency)
	 *	(Based on measurements taken with a DECstation 5000/200
	 *	 inside a Mach kernel.)
	 *
	 * 	aligned   -> aligned		(mostly)
	 * 	unaligned -> aligned		(sometimes)
	 * 	aligned,unaligned -> unaligned	(almost never)
	 *
	 *	Note that we could add another case that checks if
	 *	the destination and source are unaligned but the
	 *	copy is alignable.  eg if src and dest are both
	 *	on a halfword boundary.
	 */
	andi	t1,DSTREG,3		# get last 3 bits of dest
	bne	t1,zero,3f
	andi	t0,SRCREG,3		# get last 3 bits of src
	bne	t0,zero,5f

	/*
	 *	Forward aligned->aligned copy, 8*4 bytes at a time.
	 */
	li	AT,-32
	and	t0,SIZEREG,AT		# count truncated to multiple of 32 */
	addu	a3,SRCREG,t0		# run fast loop up to this address
	sltu	AT,SRCREG,a3		# any work to do?
	beq	AT,zero,2f
	subu	SIZEREG,t0

	/*
	 *	loop body
	 */
1:	# cp
	lw	t3,0(SRCREG)
	lw	v1,4(SRCREG)
	lw	t0,8(SRCREG)
	lw	t1,12(SRCREG)
	addu	SRCREG,32
	sw	t3,0(DSTREG)
	sw	v1,4(DSTREG)
	sw	t0,8(DSTREG)
	sw	t1,12(DSTREG)
	lw	t1,-4(SRCREG)
	lw	t0,-8(SRCREG)
	lw	v1,-12(SRCREG)
	lw	t3,-16(SRCREG)
	addu	DSTREG,32
	sw	t1,-4(DSTREG)
	sw	t0,-8(DSTREG)
	sw	v1,-12(DSTREG)
	bne	SRCREG,a3,1b
	sw	t3,-16(DSTREG)

	/*
	 *	Copy a word at a time, no loop unrolling.
	 */
2:	# wordcopy
	andi	t2,SIZEREG,3		# get byte count / 4
	subu	t2,SIZEREG,t2		# t2 = number of words to copy * 4
	beq	t2,zero,3f
	addu	t0,SRCREG,t2		# stop at t0
	subu	SIZEREG,SIZEREG,t2
1:
	lw	t3,0(SRCREG)
	addu	SRCREG,4
	sw	t3,0(DSTREG)
	bne	SRCREG,t0,1b
	addu	DSTREG,4

3:	# bytecopy
	beq	SIZEREG,zero,4f	# nothing left to do?
	nop
1:
	lb	t3,0(SRCREG)
	addu	SRCREG,1
	sb	t3,0(DSTREG)
	subu	SIZEREG,1
	bgtz	SIZEREG,1b
	addu	DSTREG,1

4:	# copydone
	j	ra
	nop

	/*
	 *	Copy from unaligned source to aligned dest.
	 */
5:	# destaligned
	andi	t0,SIZEREG,3		# t0 = bytecount mod 4
	subu	a3,SIZEREG,t0		# number of words to transfer
	beq	a3,zero,3b
	nop
	move	SIZEREG,t0		# this many to do after we are done
	addu	a3,SRCREG,a3		# stop point

1:
#ifdef __MIPSEB__
	lwl	t3,0(SRCREG)
	lwr	t3,3(SRCREG)
#else
	lwr	t3,0(SRCREG)
	lwl	t3,3(SRCREG)
#endif
	addi	SRCREG,4
	sw	t3,0(DSTREG)
	bne	SRCREG,a3,1b
	addi	DSTREG,4

	j	3b
	nop

6:	# backcopy -- based on above
	addu	SRCREG,SIZEREG
	addu	DSTREG,SIZEREG
	andi	t1,DSTREG,3		# get last 3 bits of dest
	bne	t1,zero,3f
	andi	t0,SRCREG,3		# get last 3 bits of src
	bne	t0,zero,5f

	/*
	 *	Forward aligned->aligned copy, 8*4 bytes at a time.
	 */
	li	AT,-32
	and	t0,SIZEREG,AT		# count truncated to multiple of 32
	beq	t0,zero,2f		# any work to do?
	subu	SIZEREG,t0
	subu	a3,SRCREG,t0

	/*
	 *	loop body
	 */
1:	# cp
	lw	t3,-16(SRCREG)
	lw	v1,-12(SRCREG)
	lw	t0,-8(SRCREG)
	lw	t1,-4(SRCREG)
	subu	SRCREG,32
	sw	t3,-16(DSTREG)
	sw	v1,-12(DSTREG)
	sw	t0,-8(DSTREG)
	sw	t1,-4(DSTREG)
	lw	t1,12(SRCREG)
	lw	t0,8(SRCREG)
	lw	v1,4(SRCREG)
	lw	t3,0(SRCREG)
	subu	DSTREG,32
	sw	t1,12(DSTREG)
	sw	t0,8(DSTREG)
	sw	v1,4(DSTREG)
	bne	SRCREG,a3,1b
	sw	t3,0(DSTREG)

	/*
	 *	Copy a word at a time, no loop unrolling.
	 */
2:	# wordcopy
	andi	t2,SIZEREG,3		# get byte count / 4
	subu	t2,SIZEREG,t2		# t2 = number of words to copy * 4
	beq	t2,zero,3f
	subu	t0,SRCREG,t2		# stop at t0
	subu	SIZEREG,SIZEREG,t2
1:
	lw	t3,-4(SRCREG)
	subu	SRCREG,4
	sw	t3,-4(DSTREG)
	bne	SRCREG,t0,1b
	subu	DSTREG,4

3:	# bytecopy
	beq	SIZEREG,zero,4f		# nothing left to do?
	nop
1:
	lb	t3,-1(SRCREG)
	subu	SRCREG,1
	sb	t3,-1(DSTREG)
	subu	SIZEREG,1
	bgtz	SIZEREG,1b
	subu	DSTREG,1

4:	# copydone
	j	ra
	nop

	/*
	 *	Copy from unaligned source to aligned dest.
	 */
5:	# destaligned
	andi	t0,SIZEREG,3		# t0 = bytecount mod 4
	subu	a3,SIZEREG,t0		# number of words to transfer
	beq	a3,zero,3b
	nop
	move	SIZEREG,t0		# this many to do after we are done
	subu	a3,SRCREG,a3		# stop point

1:
#ifdef __MIPSEB__
	lwl	t3,-4(SRCREG)
	lwr	t3,-1(SRCREG)
#else
	lwr	t3,-4(SRCREG)
	lwl	t3,-1(SRCREG)
#endif
	subu	SRCREG,4
	sw	t3,-4(DSTREG)
	bne	SRCREG,a3,1b
	subu	DSTREG,4

	j	3b
	nop

	.set	reorder
	.set	at
	END(FUNCTION)
