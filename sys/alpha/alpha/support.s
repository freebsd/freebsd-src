/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <machine/asm.h>
#include <assym.s>

	.text

/**************************************************************************/
	
/*
 * fu{byte,word} : fetch a byte (word) from user memory
 */
	
	LEAF(suword, 1)
	LDGP(pv)

	ldiq	t0, VM_MAXUSER_ADDRESS /* verify address validity */
	cmpult	a0, t0, t1
	beq	t1, fusufault

	lda	t0, fusufault		/* trap faults */
	ldq	t2, GD_CURPROC(globalp)
	ldq	t2, P_ADDR(t2)
	stq	t0, U_PCB_ONFAULT(t2)

	stq	a1, 0(a0)		/* try the store */

	stq	zero, U_PCB_ONFAULT(t2)	/* clean up */

	mov	zero, v0
	RET
	END(suword)
	
	LEAF(subyte, 1)
	LDGP(pv)

	ldiq	t0, VM_MAXUSER_ADDRESS /* verify address validity */
	cmpult	a0, t0, t1
	beq	t1, fusufault

	lda	t0, fusufault		/* trap faults */
	ldq	t2, GD_CURPROC(globalp)
	ldq	t2, P_ADDR(t2)
	stq	t0, U_PCB_ONFAULT(t2)

	zap	a1, 0xfe, a1		/* mask off the byte to store */
	insbl	a1, a0, a1		/* shift it to the right place */
	ldq_u	t0, 0(a0)		/* read the qword to store it in */
	mskbl	t0, a0, t0		/* make a place for our byte */
	or	a1, t0, a1		/* move it in */
	stq_u	a1, 0(a0)		/* and put the byte back */

	stq	zero, U_PCB_ONFAULT(t2)	/* clean up */

	mov	zero, v0
	RET
	END(subyte)

	LEAF(fuword, 1)
	LDGP(pv)

	ldiq	t0, VM_MAXUSER_ADDRESS /* verify address validity */
	cmpult	a0, t0, t1
	beq	t1, fusufault

	lda	t0, fusufault		/* trap faults */
	ldq	t2, GD_CURPROC(globalp)
	ldq	t2, P_ADDR(t2)
	stq	t0, U_PCB_ONFAULT(t2)

	ldq	v0, 0(a0)		/* try the fetch */

	stq	zero, U_PCB_ONFAULT(t2)	/* clean up */

	RET
	END(fuword)

	LEAF(fubyte, 1)
	LDGP(pv)

	ldiq	t0, VM_MAXUSER_ADDRESS /* verify address validity */
	cmpult	a0, t0, t1
	beq	t1, fusufault

	lda	t0, fusufault		/* trap faults */
	ldq	t2, GD_CURPROC(globalp)
	ldq	t2, P_ADDR(t2)
	stq	t0, U_PCB_ONFAULT(t2)

	ldq_u	v0, 0(a0)		/* get the word containing our byte */
	extbl	v0, a0, v0		/* extract the byte */

	stq	zero, U_PCB_ONFAULT(t2)	/* clean up */

	RET
	END(fubyte)
	
	LEAF(suibyte, 2)
	ldiq	v0, -1
	RET
	END(suibyte)

	LEAF(fusufault, 0)
	ldq	t0, GD_CURPROC(globalp)
	ldq	t0, P_ADDR(t0)
	stq	zero, U_PCB_ONFAULT(t0)
	ldiq	v0, -1
	RET
	END(fusufault)
	
LEAF(fswintrberr, 0)
XLEAF(fuswintr, 2)				/* XXX what is a 'word'? */
XLEAF(suswintr, 2)				/* XXX what is a 'word'? */
	LDGP(pv)
	ldiq	v0, -1
	RET
	END(fswintrberr)
	
/**************************************************************************/

/*
 * Copy a null-terminated string within the kernel's address space.
 * If lenp is not NULL, store the number of chars copied in *lenp
 *
 * int copystr(char *from, char *to, size_t len, size_t *lenp);
 */
LEAF(copystr, 4)
	LDGP(pv)

	mov	a2, t0			/* t0 = i = len */
	beq	a2, Lcopystr2		/* if (len == 0), bail out */

Lcopystr1:
	ldq_u	t1, 0(a0)		/* t1 = *from */
	extbl	t1, a0, t1
	ldq_u	t3, 0(a1)		/* set up t2 with quad around *to */
	insbl	t1, a1, t2
	mskbl	t3, a1, t3
	or	t3, t2, t3		/* add *from to quad around *to */
	stq_u	t3, 0(a1)		/* write out that quad */

	subl	a2, 1, a2		/* len-- */
	beq	t1, Lcopystr2		/* if (*from == 0), bail out */
	addq	a1, 1, a1		/* to++ */
	addq	a0, 1, a0		/* from++ */
	bne	a2, Lcopystr1		/* if (len != 0) copy more */

Lcopystr2:
	beq	a3, Lcopystr3		/* if (lenp != NULL) */
	subl	t0, a2, t0		/* *lenp = (i - len) */
	stq	t0, 0(a3)
Lcopystr3:
	beq	t1, Lcopystr4		/* *from == '\0'; leave quietly */

	ldiq	v0, ENAMETOOLONG		/* *from != '\0'; error. */
	RET

Lcopystr4:
	mov	zero, v0		/* return 0. */
	RET
	END(copystr)

NESTED(copyinstr, 4, 16, ra, 0, 0)
	LDGP(pv)
	lda	sp, -16(sp)			/* set up stack frame	     */
	stq	ra, (16-8)(sp)			/* save ra		     */
	ldiq	t0, VM_MAXUSER_ADDRESS		/* make sure that src addr   */
	cmpult	a0, t0, t1			/* is in user space.	     */
	beq	t1, copyerr			/* if it's not, error out.   */
	lda	v0, copyerr			/* set up fault handler.     */
	.set noat
	ldq	at_reg, GD_CURPROC(globalp)
	ldq	at_reg, P_ADDR(at_reg)
	stq	v0, U_PCB_ONFAULT(at_reg)
	.set at
	CALL(copystr)				/* do the copy.		     */
	.set noat
	ldq	at_reg, GD_CURPROC(globalp)	/* kill the fault handler.   */
	ldq	at_reg, P_ADDR(at_reg)
	stq	zero, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	RET					/* v0 left over from copystr */
	END(copyinstr)

NESTED(copyoutstr, 4, 16, ra, 0, 0)
	LDGP(pv)
	lda	sp, -16(sp)			/* set up stack frame	     */
	stq	ra, (16-8)(sp)			/* save ra		     */
	ldiq	t0, VM_MAXUSER_ADDRESS		/* make sure that dest addr  */
	cmpult	a1, t0, t1			/* is in user space.	     */
	beq	t1, copyerr			/* if it's not, error out.   */
	lda	v0, copyerr			/* set up fault handler.     */
	.set noat
	ldq	at_reg, GD_CURPROC(globalp)
	ldq	at_reg, P_ADDR(at_reg)
	stq	v0, U_PCB_ONFAULT(at_reg)
	.set at
	CALL(copystr)				/* do the copy.		     */
	.set noat
	ldq	at_reg, GD_CURPROC(globalp)	/* kill the fault handler.   */
	ldq	at_reg, P_ADDR(at_reg)
	stq	zero, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	RET					/* v0 left over from copystr */
	END(copyoutstr)

/*
 * Alternative memory mover
 */
	LEAF(memcpy,3)
	mov	a0,t0
	mov	a1,a0
	mov	t0,a1
	br	bcopy
	END(memcpy)
	
/*
 * Copy a bytes within the kernel's address space.
 *
 * In the kernel, bcopy() doesn't have to handle the overlapping
 * case; that's that ovbcopy() is for.  However, it doesn't hurt
 * to do both in bcopy, and it does provide a measure of safety.
 *
 * void bcopy(char *from, char *to, size_t len);
 * void ovbcopy(char *from, char *to, size_t len);
 */
LEAF(bcopy,3)
XLEAF(ovbcopy,3)

	/* Check for negative length */
	ble	a2,bcopy_done

	/* Check for overlap */
	subq	a1,a0,t5
	cmpult	t5,a2,t5
	bne	t5,bcopy_overlap

	/* a3 = end address */
	addq	a0,a2,a3

	/* Get the first word */
	ldq_u	t2,0(a0)

	/* Do they have the same alignment? */
	xor	a0,a1,t0
	and	t0,7,t0
	and	a1,7,t1
	bne	t0,bcopy_different_alignment

	/* src & dst have same alignment */
	beq	t1,bcopy_all_aligned

	ldq_u	t3,0(a1)
	addq	a2,t1,a2
	mskqh	t2,a0,t2
	mskql	t3,a0,t3
	or	t2,t3,t2

	/* Dst is 8-byte aligned */

bcopy_all_aligned:
	/* If less than 8 bytes,skip loop */
	subq	a2,1,t0
	and	a2,7,a2
	bic	t0,7,t0
	beq	t0,bcopy_samealign_lp_end

bcopy_samealign_lp:
	stq_u	t2,0(a1)
	addq	a1,8,a1
	ldq_u	t2,8(a0)
	subq	t0,8,t0
	addq	a0,8,a0
	bne	t0,bcopy_samealign_lp

bcopy_samealign_lp_end:
	/* If we're done, exit */
	bne	a2,bcopy_small_left
	stq_u	t2,0(a1)
	RET

bcopy_small_left:
	mskql	t2,a2,t4
	ldq_u	t3,0(a1)
	mskqh	t3,a2,t3
	or	t4,t3,t4
	stq_u	t4,0(a1)
	RET

bcopy_different_alignment:
	/*
	 * this is the fun part
	 */
	addq	a0,a2,a3
	cmpule	a2,8,t0
	bne	t0,bcopy_da_finish

	beq	t1,bcopy_da_noentry

	/* Do the initial partial word */
	subq	zero,a1,t0
	and	t0,7,t0
	ldq_u	t3,7(a0)
	extql	t2,a0,t2
	extqh	t3,a0,t3
	or	t2,t3,t5
	insql	t5,a1,t5
	ldq_u	t6,0(a1)
	mskql	t6,a1,t6
	or	t5,t6,t5
	stq_u	t5,0(a1)
	addq	a0,t0,a0
	addq	a1,t0,a1
	subq	a2,t0,a2
	ldq_u	t2,0(a0)

bcopy_da_noentry:
	subq	a2,1,t0
	bic	t0,7,t0
	and	a2,7,a2
	beq	t0,bcopy_da_finish2

bcopy_da_lp:
	ldq_u	t3,7(a0)
	addq	a0,8,a0
	extql	t2,a0,t4
	extqh	t3,a0,t5
	subq	t0,8,t0
	or	t4,t5,t5
	stq	t5,0(a1)
	addq	a1,8,a1
	beq	t0,bcopy_da_finish1
	ldq_u	t2,7(a0)
	addq	a0,8,a0
	extql	t3,a0,t4
	extqh	t2,a0,t5
	subq	t0,8,t0
	or	t4,t5,t5
	stq	t5,0(a1)
	addq	a1,8,a1
	bne	t0,bcopy_da_lp

bcopy_da_finish2:
	/* Do the last new word */
	mov	t2,t3

bcopy_da_finish1:
	/* Do the last partial word */
	ldq_u	t2,-1(a3)
	extql	t3,a0,t3
	extqh	t2,a0,t2
	or	t2,t3,t2
	br	zero,bcopy_samealign_lp_end

bcopy_da_finish:
	/* Do the last word in the next source word */
	ldq_u	t3,-1(a3)
	extql	t2,a0,t2
	extqh	t3,a0,t3
	or	t2,t3,t2
	insqh	t2,a1,t3
	insql	t2,a1,t2
	lda	t4,-1(zero)
	mskql	t4,a2,t5
	cmovne	t5,t5,t4
	insqh	t4,a1,t5
	insql	t4,a1,t4
	addq	a1,a2,a4
	ldq_u	t6,0(a1)
	ldq_u	t8,-1(a4)
	bic	t6,t4,t6
	bic	t8,t5,t8
	and	t2,t4,t2
	and	t3,t5,t3
	or	t2,t6,t2
	or	t3,t8,t3
	stq_u	t3,-1(a4)
	stq_u	t2,0(a1)
	RET

bcopy_overlap:
	/*
	 * Basically equivalent to previous case, only backwards.
	 * Not quite as highly optimized
	 */
	addq	a0,a2,a3
	addq	a1,a2,a4

	/* less than 8 bytes - don't worry about overlap */
	cmpule	a2,8,t0
	bne	t0,bcopy_ov_short

	/* Possibly do a partial first word */
	and	a4,7,t4
	beq	t4,bcopy_ov_nostart2
	subq	a3,t4,a3
	subq	a4,t4,a4
	ldq_u	t1,0(a3)
	subq	a2,t4,a2
	ldq_u	t2,7(a3)
	ldq	t3,0(a4)
	extql	t1,a3,t1
	extqh	t2,a3,t2
	or	t1,t2,t1
	mskqh	t3,t4,t3
	mskql	t1,t4,t1
	or	t1,t3,t1
	stq	t1,0(a4)

bcopy_ov_nostart2:
	bic	a2,7,t4
	and	a2,7,a2
	beq	t4,bcopy_ov_lp_end

bcopy_ov_lp:
	/* This could be more pipelined, but it doesn't seem worth it */
	ldq_u	t0,-8(a3)
	subq	a4,8,a4
	ldq_u	t1,-1(a3)
	subq	a3,8,a3
	extql	t0,a3,t0
	extqh	t1,a3,t1
	subq	t4,8,t4
	or	t0,t1,t0
	stq	t0,0(a4)
	bne	t4,bcopy_ov_lp

bcopy_ov_lp_end:
	beq	a2,bcopy_done

	ldq_u	t0,0(a0)
	ldq_u	t1,7(a0)
	ldq_u	t2,0(a1)
	extql	t0,a0,t0
	extqh	t1,a0,t1
	or	t0,t1,t0
	insql	t0,a1,t0
	mskql	t2,a1,t2
	or	t2,t0,t2
	stq_u	t2,0(a1)

bcopy_done:
	RET

bcopy_ov_short:
	ldq_u	t2,0(a0)
	br	zero,bcopy_da_finish

	END(bcopy)
	
NESTED(copyin, 3, 16, ra, 0, 0)
	LDGP(pv)
	lda	sp, -16(sp)			/* set up stack frame	     */
	stq	ra, (16-8)(sp)			/* save ra		     */
	ldiq	t0, VM_MAXUSER_ADDRESS		/* make sure that src addr   */
	cmpult	a0, t0, t1			/* is in user space.	     */
	beq	t1, copyerr			/* if it's not, error out.   */
	lda	v0, copyerr			/* set up fault handler.     */
	.set noat
	ldq	at_reg, GD_CURPROC(globalp)
	ldq	at_reg, P_ADDR(at_reg)
	stq	v0, U_PCB_ONFAULT(at_reg)
	.set at
	CALL(bcopy)				/* do the copy.		     */
	.set noat
	ldq	at_reg, GD_CURPROC(globalp)	/* kill the fault handler.   */
	ldq	at_reg, P_ADDR(at_reg)
	stq	zero, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	mov	zero, v0			/* return 0. */
	RET
	END(copyin)

NESTED(copyout, 3, 16, ra, 0, 0)
	LDGP(pv)
	lda	sp, -16(sp)			/* set up stack frame	     */
	stq	ra, (16-8)(sp)			/* save ra		     */
	ldiq	t0, VM_MAXUSER_ADDRESS		/* make sure that dest addr  */
	cmpult	a1, t0, t1			/* is in user space.	     */
	beq	t1, copyerr			/* if it's not, error out.   */
	lda	v0, copyerr			/* set up fault handler.     */
	.set noat
	ldq	at_reg, GD_CURPROC(globalp)
	ldq	at_reg, P_ADDR(at_reg)
	stq	v0, U_PCB_ONFAULT(at_reg)
	.set at
	CALL(bcopy)				/* do the copy.		     */
	.set noat
	ldq	at_reg, GD_CURPROC(globalp)	/* kill the fault handler.   */
	ldq	at_reg, P_ADDR(at_reg)
	stq	zero, U_PCB_ONFAULT(at_reg)
	.set at
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	mov	zero, v0			/* return 0. */
	RET
	END(copyout)

LEAF(copyerr, 0)
	ldq	t0, GD_CURPROC(globalp)
	ldq	t0, P_ADDR(t0)
	stq	zero, U_PCB_ONFAULT(t0)		/* reset fault handler.	     */
	ldq	ra, (16-8)(sp)			/* restore ra.		     */
	lda	sp, 16(sp)			/* kill stack frame.	     */
	ldiq	v0, EFAULT			/* return EFAULT.	     */
	RET
END(copyerr)
	
/**************************************************************************/

/*
 * Kernel setjmp and longjmp.  Rather minimalist.
 *
 *	longjmp(label_t *a)
 * will generate a "return (1)" from the last call to
 *	setjmp(label_t *a)
 * by restoring registers from the stack,
 */

	.set	noreorder

LEAF(setjmp, 1)
	LDGP(pv)

	stq	ra, (0 * 8)(a0)			/* return address */
	stq	s0, (1 * 8)(a0)			/* callee-saved registers */
	stq	s1, (2 * 8)(a0)
	stq	s2, (3 * 8)(a0)
	stq	s3, (4 * 8)(a0)
	stq	s4, (5 * 8)(a0)
	stq	s5, (6 * 8)(a0)
	stq	s6, (7 * 8)(a0)
	stq	sp, (8 * 8)(a0)

	ldiq	t0, 0xbeeffedadeadbabe		/* set magic number */
	stq	t0, (9 * 8)(a0)

	mov	zero, v0			/* return zero */
	RET
END(setjmp)

LEAF(longjmp, 1)
	LDGP(pv)

	ldiq	t0, 0xbeeffedadeadbabe		/* check magic number */
	ldq	t1, (9 * 8)(a0)
	cmpeq	t0, t1, t0
	beq	t0, longjmp_botch		/* if bad, punt */

	ldq	ra, (0 * 8)(a0)			/* return address */
	ldq	s0, (1 * 8)(a0)			/* callee-saved registers */
	ldq	s1, (2 * 8)(a0)
	ldq	s2, (3 * 8)(a0)
	ldq	s3, (4 * 8)(a0)
	ldq	s4, (5 * 8)(a0)
	ldq	s5, (6 * 8)(a0)
	ldq	s6, (7 * 8)(a0)
	ldq	sp, (8 * 8)(a0)

	ldiq	v0, 1
	RET

longjmp_botch:
	lda	a0, longjmp_botchmsg
	mov	ra, a1
	CALL(panic)
	call_pal PAL_bugchk

	.data
longjmp_botchmsg:
	.asciz	"longjmp botch from %p"
	.text
END(longjmp)
