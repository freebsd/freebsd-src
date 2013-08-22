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
/*-
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
#include <machine/ia64_cpu.h>
#include <assym.s>

	.text

ENTRY(fusufault, 0)
{	.mib
	st8.rel		[r15]=r0		// Clear onfault.
	add		ret0=-1,r0
	br.ret.sptk	rp
	;;
}
END(fusufault)

/*
 * casuword(u_long *p, u_long old, u_long new)
 *	Perform a compare-exchange in user space.
 */
ENTRY(casuword, 3)
{	.mlx
	ld8.acq		r15=[r13]		// r15 = curthread
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	add		r15=TD_PCB,r15
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	ld8		r15=[r15]		// r15 = PCB
	movl		r14=fusufault
	;;
}
{	.mmi
	mov		ar.ccv=in1
	add		r15=PCB_ONFAULT,r15
	nop		0
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	cmpxchg8.rel	ret0=[in0],in2,ar.ccv
	nop		0
	;;
}
{	.mib
	st8.rel		[r15]=r0		// Clear onfault
	nop		0
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(casuword)

/*
 * casuword32(uint32_t *p, uint32_t old, uint32_t new)
 *	Perform a 32-bit compare-exchange in user space.
 */
ENTRY(casuword32, 3)
{	.mlx
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	ld8.acq		r15=[r13]		// r15 = curthread
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	add		r15=TD_PCB,r15
	movl		r14=fusufault
	;;
}
{	.mmi
	ld8		r15=[r15]		// r15 = PCB
	;;
	mov		ar.ccv=in1
	add		r15=PCB_ONFAULT,r15
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	cmpxchg4.rel	ret0=[in0],in2,ar.ccv
	nop		0
	;;
}
{	.mib
	st8.rel		[r15]=r0		// Clear onfault
	nop		0
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(casuword32)

/*
 * subyte(void *addr, int byte)
 * suword16(void *addr, int word)
 * suword32(void *addr, int word)
 * suword64|suword(void *addr, long word)
 *	Store in user space
 */

ENTRY(subyte, 2)
{	.mlx
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	ld8.acq		r15=[r13]		// r15 = curthread
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	add		r15=TD_PCB,r15
	movl		r14=fusufault
	;;
}
{	.mmi
	ld8		r15=[r15]		// r15 = PCB
	;;
	nop		0
	add		r15=PCB_ONFAULT,r15
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	st1.rel		[in0]=in1
	nop		0
	;;
}
{	.mib
	st8.rel		[r15]=r0		// Clear onfault
	mov		ret0=r0
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(subyte)

ENTRY(suword16, 2)
{	.mlx
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	ld8.acq		r15=[r13]		// r15 = curthread
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	add		r15=TD_PCB,r15
	movl		r14=fusufault
	;;
}
{	.mmi
	ld8		r15=[r15]		// r15 = PCB
	;;
	nop		0
	add		r15=PCB_ONFAULT,r15
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	st2.rel		[in0]=in1
	nop		0
	;;
}
{	.mib
	st8.rel		[r15]=r0		// Clear onfault
	mov		ret0=r0
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(suword16)

ENTRY(suword32, 2)
{	.mlx
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	ld8.acq		r15=[r13]		// r15 = curthread
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	add		r15=TD_PCB,r15
	movl		r14=fusufault
	;;
}
{	.mmi
	ld8		r15=[r15]		// r15 = PCB
	;;
	nop		0
	add		r15=PCB_ONFAULT,r15
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	st4.rel		[in0]=in1
	nop		0
	;;
}
{	.mib
	st8.rel		[r15]=r0		// Clear onfault
	mov		ret0=r0
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(suword32)

ENTRY(suword64, 2)
XENTRY(suword)
{	.mlx
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	ld8.acq		r15=[r13]		// r15 = curthread
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	add		r15=TD_PCB,r15
	movl		r14=fusufault
	;;
}
{	.mmi
	ld8		r15=[r15]		// r15 = PCB
	;;
	nop		0
	add		r15=PCB_ONFAULT,r15
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	st8.rel		[in0]=in1
	nop		0
	;;
}
{	.mib
	st8.rel		[r15]=r0		// Clear onfault
	mov		ret0=r0
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(suword64)

/*
 * fubyte(void *addr, int byte)
 * fuword16(void *addr, int word)
 * fuword32(void *addr, int word)
 * fuword64|fuword(void *addr, long word)
 *	Fetch from user space
 */

ENTRY(fubyte, 1)
{	.mlx
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	ld8.acq		r15=[r13]		// r15 = curthread
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	add		r15=TD_PCB,r15
	movl		r14=fusufault
	;;
}
{	.mmi
	ld8		r15=[r15]		// r15 = PCB
	;;
	nop		0
	add		r15=PCB_ONFAULT,r15
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	mf
	nop		0
	;;
}
{	.mmb
	ld1		ret0=[in0]
	st8.rel		[r15]=r0		// Clear onfault
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(fubyte)

ENTRY(fuword16, 2)
{	.mlx
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	ld8.acq		r15=[r13]		// r15 = curthread
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	add		r15=TD_PCB,r15
	movl		r14=fusufault
	;;
}
{	.mmi
	ld8		r15=[r15]		// r15 = PCB
	;;
	nop		0
	add		r15=PCB_ONFAULT,r15
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	mf
	nop		0
	;;
}
{	.mmb
	ld2		ret0=[in0]
	st8.rel		[r15]=r0		// Clear onfault
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(fuword16)

ENTRY(fuword32, 2)
{	.mlx
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	ld8.acq		r15=[r13]		// r15 = curthread
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	add		r15=TD_PCB,r15
	movl		r14=fusufault
	;;
}
{	.mmi
	ld8		r15=[r15]		// r15 = PCB
	;;
	nop		0
	add		r15=PCB_ONFAULT,r15
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	mf
	nop		0
	;;
}
{	.mmb
	ld4		ret0=[in0]
	st8.rel		[r15]=r0		// Clear onfault
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(fuword32)

ENTRY(fuword64, 2)
XENTRY(fuword)
{	.mlx
	movl		r14=VM_MAXUSER_ADDRESS
	;;
}
{	.mib
	ld8.acq		r15=[r13]		// r15 = curthread
	cmp.geu		p6,p0=in0,r14
(p6)	br.dpnt.few	1f
	;;
}
{	.mlx
	add		r15=TD_PCB,r15
	movl		r14=fusufault
	;;
}
{	.mmi
	ld8		r15=[r15]		// r15 = PCB
	;;
	nop		0
	add		r15=PCB_ONFAULT,r15
	;;
}
{	.mmi
	st8		[r15]=r14		// Set onfault
	;;
	mf
	nop		0
	;;
}
{	.mmb
	ld8		ret0=[in0]
	st8.rel		[r15]=r0		// Clear onfault
	br.ret.sptk	rp
	;;
}
1:
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(fuword64)

/*
 * fuswintr(void *addr)
 * suswintr(void *addr)
 */

ENTRY(fuswintr, 1)
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(fuswintr)

ENTRY(suswintr, 0)
{	.mib
	add		ret0=-1,r0
	nop		0
	br.ret.sptk	rp
	;;
}
END(suswintr)

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
	sub	r14=r14,in2		// *lenp = (i - len)
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
	.prologue
	.regstk	4, 3, 4, 0
	.save	ar.pfs,loc0
	alloc	loc0=ar.pfs,4,3,4,0
	.save	rp,loc1
	mov	loc1=rp
	.body

	movl	loc2=VM_MAXUSER_ADDRESS		// make sure that src addr
	;; 
	cmp.geu	p6,p0=in0,loc2			// is in user space.
	;; 
(p6)	br.cond.spnt.few copyerr		// if it's not, error out.
	ld8.acq	r15=[r13]
	movl	r14=copyerr			// set up fault handler.
	;;
	add	r15=TD_PCB,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	loc2=PCB_ONFAULT,r15
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
	mov	ar.pfs=loc0			// restore ar.pfs
	mov	rp=loc1				// restore ra.
	br.ret.sptk.few rp			// ret0 left over from copystr
END(copyinstr)

/*
 * Not the fastest bcopy in the world.
 */
ENTRY(bcopy, 3)
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
	.prologue
	.regstk	3, 3, 3, 0
	.save	ar.pfs,loc0
	alloc	loc0=ar.pfs,3,3,3,0
	.save	rp,loc1
	mov	loc1=rp
	.body

	movl	loc2=VM_MAXUSER_ADDRESS		// make sure that src addr
	;; 
	cmp.geu	p6,p0=in0,loc2			// is in user space.
	;; 
(p6)	br.cond.spnt.few copyerr		// if it's not, error out.
	ld8.acq	r15=[r13]
	movl	r14=copyerr			// set up fault handler.
	;;
	add	r15=TD_PCB,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	loc2=PCB_ONFAULT,r15
	;;
	st8	[loc2]=r14
	;;
	mov	out0=in0
	mov	out1=in1
	mov	out2=in2
	;;
	br.call.sptk.few rp=bcopy		// do the copy.
	st8	[loc2]=r0			// kill the fault handler.
	mov	ar.pfs=loc0			// restore ar.pfs
	mov	rp=loc1				// restore ra.
	br.ret.sptk.few rp			// ret0 left over from bcopy
END(copyin)

ENTRY(copyout, 3)
	.prologue
	.regstk	3, 3, 3, 0
	.save	ar.pfs,loc0
	alloc	loc0=ar.pfs,3,3,3,0
	.save	rp,loc1
	mov	loc1=rp
	.body

	movl	loc2=VM_MAXUSER_ADDRESS		// make sure that dest addr
	;; 
	cmp.geu	p6,p0=in1,loc2			// is in user space.
	;; 
(p6)	br.cond.spnt.few copyerr		// if it's not, error out.
	ld8.acq	r15=[r13]
	movl	r14=copyerr			// set up fault handler.
	;;
	add	r15=TD_PCB,r15			// find pcb
	;;
	ld8	r15=[r15]
	;;
	add	loc2=PCB_ONFAULT,r15
	;;
	st8	[loc2]=r14
	;;
	mov	out0=in0
	mov	out1=in1
	mov	out2=in2
	;;
	br.call.sptk.few rp=bcopy		// do the copy.
	st8	[loc2]=r0			// kill the fault handler.
	mov	ar.pfs=loc0			// restore ar.pfs
	mov	rp=loc1				// restore ra.
	br.ret.sptk.few rp			// ret0 left over from bcopy
END(copyout)

ENTRY(copyerr, 0)
	ld8.acq	r14=[r13] ;;
	add	r14=TD_PCB,r14 ;;		// curthread->td_addr
	ld8	r14=[r14] ;;
	add	r14=PCB_ONFAULT,r14 ;;		// &curthread->td_pcb->pcb_onfault
	st8	[r14]=r0			// reset fault handler
	
	mov	ret0=EFAULT			// return EFAULT
	br.ret.sptk.few rp
END(copyerr)

#if defined(GPROF)
/*
 * Important registers:
 *      r8      structure return address
 *      rp      our return address
 *      in0     caller's ar.pfs
 *      in1     caller's gp
 *      in2     caller's rp
 *      in3     GOT entry
 *      ar.pfs  our pfs
 */
ENTRY_NOPROFILE(_mcount, 4)
	alloc		loc0 = ar.pfs, 4, 3, 2, 0
	mov		loc1 = r8
	mov		loc2 = rp
	;;
	mov		out0 = in2
	mov		out1 = rp
	br.call.sptk	rp = __mcount
	;;
1:
	mov		gp = in1
	mov		r14 = ip
	mov		b7 = loc2
	;;
	add		r14 = 2f - 1b, r14
	mov		ar.pfs = loc0
	mov		rp = in2
	;;
	mov		b7 = r14
	mov		b6 = loc2
	mov		r8 = loc1
	mov		r14 = in0
	br.ret.sptk	b7
	;;
2:
	mov		ar.pfs = r14
	br.sptk		b6
	;;
END(_mcount)
#endif
