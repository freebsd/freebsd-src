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
	
ENTRY(suword, 2)

	movl	r14=VM_MAXUSER_ADDRESS;;	// make sure address is ok
	cmp.geu	p6,p0=in0,r14
(p6)	br.dpnt.few fusufault

	movl	r14=fusufault			// set up fault handler.
	add	r15=GD_CURPROC,r13		// find curproc
	;;
	ld8	r15=[r15]
	;;
	add	r15=P_ADDR,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	r15=U_PCB_ONFAULT,r15
	;;
	st8	[r15]=r14
	;;
	st8.rel	[in0]=in1			// try the store
	;;
	st8	[r15]=r0			// clean up

	mov	ret0=r0
	br.ret.sptk.few rp

END(suword)
	
ENTRY(subyte, 2)

	movl	r14=VM_MAXUSER_ADDRESS;;	// make sure address is ok
	cmp.geu	p6,p0=in0,r14
(p6)	br.dpnt.few fusufault

	movl	r14=fusufault			// set up fault handler.
	add	r15=GD_CURPROC,r13		// find curproc
	;;
	ld8	r15=[r15]
	;;
	add	r15=P_ADDR,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	r15=U_PCB_ONFAULT,r15
	;;
	st8	[r15]=r14
	;;
	st1.rel	[in0]=in1			// try the store
	;;
	st8	[r15]=r0			// clean up

	mov	ret0=r0
	br.ret.sptk.few rp

END(subyte)

ENTRY(fuword, 1)

	movl	r14=VM_MAXUSER_ADDRESS;;	// make sure address is ok
	cmp.geu	p6,p0=in0,r14
(p6)	br.dpnt.few fusufault

	movl	r14=fusufault			// set up fault handler.
	add	r15=GD_CURPROC,r13		// find curproc
	;;
	ld8	r15=[r15]
	;;
	add	r15=P_ADDR,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	r15=U_PCB_ONFAULT,r15
	;;
	st8	[r15]=r14
	;;
	ld8.acq	ret0=[in0]			// try the fetch
	;;
	st8	[r15]=r0			// clean up

	br.ret.sptk.few rp

END(fuword)

ENTRY(fubyte, 1)

	movl	r14=VM_MAXUSER_ADDRESS;;	// make sure address is ok
	cmp.geu	p6,p0=in0,r14
(p6)	br.dpnt.few fusufault

	movl	r14=fusufault			// set up fault handler.
	add	r15=GD_CURPROC,r13		// find curproc
	;;
	ld8	r15=[r15]
	;;
	add	r15=P_ADDR,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	r15=U_PCB_ONFAULT,r15
	;;
	st8	[r15]=r14
	;;
	ld1.acq	ret0=[in0]			// try the fetch
	;;
	st8	[r15]=r0			// clean up

	br.ret.sptk.few rp

END(fubyte)
	
ENTRY(suibyte, 2)
	mov	ret0=-1
	br.ret.sptk.few rp
END(suibyte)

ENTRY(fusufault, 0)
	st8	[r15]=r0 ;;			// r15 points at onfault
	mov	ret0=r0
	br.ret.sptk.few rp
END(fusufault)
	
ENTRY(fswintrberr, 0)
XENTRY(fuswintr)					/* XXX what is a 'word'? */
XENTRY(suswintr)					/* XXX what is a 'word'? */
	mov	ret0=-1
	br.ret.sptk.few rp
END(fswintrberr)
	
/**************************************************************************/

/*
 * Copy a null-terminated string within the kernel's address space.
 * If lenp is not NULL, store the number of chars copied in *lenp
 *
 * int copystr(char *from, char *to, size_t len, size_t *lenp);
 */
ENTRY(copystr, 4)
	mov	r14=in2			// r14 = i = len
	cmp.eq	p6,p0=r0,in2
(p6)	br.cond.spnt.few 2f		// if (len == 0), bail out

1:	ld1	r15=[in0],1		// read one byte
	;;
	st1	[in1]=r15,1		// write that byte
	add	in2=-1,in2		// len--
	;;
	cmp.eq	p6,p0=r0,r15
	cmp.ne	p7,p0=r0,in2
	;; 
(p6)	br.cond.spnt.few 2f		// if (*from == 0), bail out
(p7)	br.cond.sptk.few 1b		// if (len != 0) copy more

2:	cmp.eq	p6,p0=r0,in3
(p6)	br.cond.dpnt.few 3f		// if (lenp != NULL)
	sub	r14=in2,r14		// *lenp = (i - len)
	;;
	st8	[in3]=r14
	
3:	cmp.eq	p6,p0=r0,r15
(p6)	br.cond.spnt.few 4f		// *from == '\0'; leave quietly

	mov	ret0=ENAMETOOLONG	// *from != '\0'; error.
	br.ret.sptk.few rp

4:	mov	ret0=0			// return 0.
	br.ret.sptk.few rp
	
END(copystr)

ENTRY(copyinstr, 4)
	alloc	loc0=ar.pfs,4,3,4,0
	mov	loc1=rp

	movl	loc2=VM_MAXUSER_ADDRESS		// make sure that src addr
	;; 
	cmp.geu	p6,p0=in0,loc2			// is in user space.
	;; 
(p6)	br.cond.spnt.few copyerr		// if it's not, error out.
	movl	r14=copyerr			// set up fault handler.
	add	r15=GD_CURPROC,r13		// find curproc
	;;
	ld8	r15=[r15]
	;;
	add	r15=P_ADDR,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	loc2=U_PCB_ONFAULT,r15
	;;
	st8	[loc2]=r14
	;;
	mov	out0=in0
	mov	out1=in1
	mov	out2=in2
	mov	out3=in3
	;;
	br.call.sptk.few rp=copystr		// do the copy.
	st8	[loc2]=r0			// kill the fault handler.
	mov	rp=loc1				// restore ra.
	br.ret.sptk.few rp			// ret0 left over from copystr

END(copyinstr)

ENTRY(copyoutstr, 4)
	alloc	loc0=ar.pfs,4,3,4,0
	mov	loc1=rp

	movl	loc2=VM_MAXUSER_ADDRESS		// make sure that dest addr
	;; 
	cmp.geu	p6,p0=in1,loc2			// is in user space.
	;; 
(p6)	br.cond.spnt.few copyerr		// if it's not, error out.
	movl	r14=copyerr			// set up fault handler.
	add	r15=GD_CURPROC,r13		// find curproc
	;;
	ld8	r15=[r15]
	;;
	add	r15=P_ADDR,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	loc2=U_PCB_ONFAULT,r15
	;;
	st8	[loc2]=r14
	;;
	mov	out0=in0
	mov	out1=in1
	mov	out2=in2
	mov	out3=in3
	;;
	br.call.sptk.few rp=copystr		// do the copy.
	st8	[loc2]=r0			// kill the fault handler.
	mov	rp=loc1				// restore ra.
	br.ret.sptk.few rp			// ret0 left over from copystr

END(copyoutstr)

/*
 * Not the fastest bcopy in the world.
 */
ENTRY(bcopy, 3)
XENTRY(ovbcopy)

	mov	ret0=r0				// return zero for copy{in,out}
	;; 
	cmp.le	p6,p0=in2,r0			// bail if len <= 0
(p6)	br.ret.spnt.few rp

	sub	r14=in1,in0 ;;			// check for overlap
	cmp.ltu	p6,p0=r14,in2			// dst-src < len
(p6)	br.cond.spnt.few 5f

	extr.u	r14=in0,0,3			// src & 7
	extr.u	r15=in1,0,3 ;;			// dst & 7
	cmp.eq	p6,p0=r14,r15			// different alignment?
(p6)	br.cond.spnt.few 2f			// branch if same alignment

1:	ld1	r14=[in0],1 ;;			// copy bytewise
	st1	[in1]=r14,1
	add	in2=-1,in2 ;;			// len--
	cmp.ne	p6,p0=r0,in2
(p6)	br.cond.dptk.few 1b			// loop
	br.ret.sptk.few rp			// done

2:	cmp.eq	p6,p0=r14,r0			// aligned?
(p6)	br.cond.sptk.few 4f

3:	ld1	r14=[in0],1 ;;			// copy bytewise
	st1	[in1]=r14,1
	extr.u	r15=in0,0,3			// src & 7
	add	in2=-1,in2 ;;			// len--
	cmp.eq	p6,p0=r0,in2			// done?
	cmp.eq	p7,p0=r0,r15 ;;			// aligned now?
(p6)	br.ret.spnt.few rp			// return if done
(p7)	br.cond.spnt.few 4f			// go to main copy
	br.cond.sptk.few 3b			// more bytes to copy

	// At this point, in2 is non-zero

4:	mov	r14=8 ;;
	cmp.ltu	p6,p0=in2,r14 ;;		// len < 8?
(p6)	br.cond.spnt.few 1b			// byte copy the end
	ld8	r15=[in0],8 ;;			// copy word
	st8	[in1]=r15,8
	add	in2=-8,in2 ;;			// len -= 8
	cmp.ne	p6,p0=r0,in2			// done?
(p6)	br.cond.spnt.few 4b			// again

	br.ret.sptk.few rp			// return

	// Don't bother optimising overlap case

5:	add	in0=in0,in2
	add	in1=in1,in2 ;;
	add	in0=-1,in0
	add	in1=-1,in1 ;;

6:	ld1	r14=[in0],-1 ;;
	st1	[in1]=r14,-1
	add	in2=-1,in2 ;;
	cmp.ne	p6,p0=r0,in2
(p6)	br.cond.spnt.few 6b

	br.ret.sptk.few rp

END(bcopy)

ENTRY(memcpy,3)
	
	mov	r14=in0 ;;
	mov	in0=in1 ;;
	mov	in1=r14
	br.cond.sptk.few bcopy
	
END(memcpy)
	
ENTRY(copyin, 3)
	
	alloc	loc0=ar.pfs,3,3,3,0
	mov	loc1=rp

	movl	loc2=VM_MAXUSER_ADDRESS		// make sure that src addr
	;; 
	cmp.ltu	p6,p0=in0,loc2			// is in user space.
	;; 
(p6)	br.cond.spnt.few copyerr		// if it's not, error out.
	movl	r14=copyerr			// set up fault handler.
	add	r15=GD_CURPROC,r13		// find curproc
	;;
	ld8	r15=[r15]
	;;
	add	r15=P_ADDR,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	loc2=U_PCB_ONFAULT,r15
	;;
	st8	[loc2]=r14
	;;
	mov	out0=in0
	mov	out1=in1
	mov	out2=in2
	;;
	br.call.sptk.few rp=bcopy		// do the copy.
	st8	[loc2]=r0			// kill the fault handler.
	mov	rp=loc1				// restore ra.
	br.ret.sptk.few rp			// ret0 left over from bcopy
	
END(copyin)

ENTRY(copyout, 3)
	
	alloc	loc0=ar.pfs,3,3,3,0
	mov	loc1=rp

	movl	loc2=VM_MAXUSER_ADDRESS		// make sure that dest addr
	;; 
	cmp.ltu	p6,p0=in1,loc2			// is in user space.
	;; 
(p6)	br.cond.spnt.few copyerr		// if it's not, error out.
	movl	r14=copyerr			// set up fault handler.
	add	r15=GD_CURPROC,r13		// find curproc
	;;
	ld8	r15=[r15]
	;;
	add	r15=P_ADDR,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	loc2=U_PCB_ONFAULT,r15
	;;
	st8	[loc2]=r14
	;;
	mov	out0=in0
	mov	out1=in1
	mov	out2=in2
	;;
	br.call.sptk.few rp=bcopy		// do the copy.
	st8	[loc2]=r0			// kill the fault handler.
	mov	rp=loc1				// restore ra.
	br.ret.sptk.few rp			// ret0 left over from bcopy
	
END(copyout)

ENTRY(copyerr, 0)

	add	r14=GD_CURPROC,r13 ;;		// find curproc
	ld8	r14=[r14] ;;
	add	r14=P_ADDR,r14 ;;		// curproc->p_addr
	ld8	r14=[r14] ;;
	add	r14=U_PCB_ONFAULT,r14 ;;	// &curproc->p_addr->u_pcb.pcb_onfault
	st8	[r14]=r0			// reset fault handler
	
	mov	ret0=EFAULT			// return EFAULT
	br.ret.sptk.few rp

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


ENTRY(setjmp, 1)
#if 0
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
#endif
END(setjmp)

ENTRY(longjmp, 1)
#if 0
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
#endif
END(longjmp)
