/*-
 * Copyright (c) 1993 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#include <machine/asmacros.h>
#include <machine/cputypes.h>
#include <machine/pmap.h>
#include <machine/specialreg.h>

#include "assym.inc"

#define IDXSHIFT	10

	.text

/*
 * bcopy family
 * void bzero(void *buf, u_int len)
 */
ENTRY(bzero)
	pushl	%edi
	movl	8(%esp),%edi
	movl	12(%esp),%ecx
	xorl	%eax,%eax
	shrl	$2,%ecx
	rep
	stosl
	movl	12(%esp),%ecx
	andl	$3,%ecx
	rep
	stosb
	popl	%edi
	ret
END(bzero)

ENTRY(sse2_pagezero)
	pushl	%ebx
	movl	8(%esp),%ecx
	movl	%ecx,%eax
	addl	$4096,%eax
	xor	%ebx,%ebx
	jmp	1f
	/*
	 * The loop takes 14 bytes.  Ensure that it doesn't cross a 16-byte
	 * cache line.
	 */
	.p2align 4,0x90
1:
	movnti	%ebx,(%ecx)
	movnti	%ebx,4(%ecx)
	addl	$8,%ecx
	cmpl	%ecx,%eax
	jne	1b
	sfence
	popl	%ebx
	ret
END(sse2_pagezero)

ENTRY(i686_pagezero)
	pushl	%edi
	pushl	%ebx

	movl	12(%esp),%edi
	movl	$1024,%ecx

	ALIGN_TEXT
1:
	xorl	%eax,%eax
	repe
	scasl
	jnz	2f

	popl	%ebx
	popl	%edi
	ret

	ALIGN_TEXT

2:
	incl	%ecx
	subl	$4,%edi

	movl	%ecx,%edx
	cmpl	$16,%ecx

	jge	3f

	movl	%edi,%ebx
	andl	$0x3f,%ebx
	shrl	%ebx
	shrl	%ebx
	movl	$16,%ecx
	subl	%ebx,%ecx

3:
	subl	%ecx,%edx
	rep
	stosl

	movl	%edx,%ecx
	testl	%edx,%edx
	jnz	1b

	popl	%ebx
	popl	%edi
	ret
END(i686_pagezero)

/* fillw(pat, base, cnt) */
ENTRY(fillw)
	pushl	%edi
	movl	8(%esp),%eax
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	rep
	stosw
	popl	%edi
	ret
END(fillw)

ENTRY(bcopyb)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax			/* overlapping && src < dst? */
	jb	1f
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:
	addl	%ecx,%edi			/* copy backwards. */
	addl	%ecx,%esi
	decl	%edi
	decl	%esi
	std
	rep
	movsb
	popl	%edi
	popl	%esi
	cld
	ret
END(bcopyb)

/*
 * bcopy(src, dst, cnt)
 *  ws@tools.de     (Wolfgang Solfrank, TooLs GmbH) +49-228-985800
 */
ENTRY(bcopy)
	pushl	%ebp
	movl	%esp,%ebp
	pushl	%esi
	pushl	%edi
	movl	8(%ebp),%esi
	movl	12(%ebp),%edi
	movl	16(%ebp),%ecx

	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax			/* overlapping && src < dst? */
	jb	1f

	shrl	$2,%ecx				/* copy by 32-bit words */
	rep
	movsl
	movl	16(%ebp),%ecx
	andl	$3,%ecx				/* any bytes left? */
	rep
	movsb
	popl	%edi
	popl	%esi
	popl	%ebp
	ret

	ALIGN_TEXT
1:
	addl	%ecx,%edi			/* copy backwards */
	addl	%ecx,%esi
	decl	%edi
	decl	%esi
	andl	$3,%ecx				/* any fractional bytes? */
	std
	rep
	movsb
	movl	16(%ebp),%ecx			/* copy remainder by 32-bit words */
	shrl	$2,%ecx
	subl	$3,%esi
	subl	$3,%edi
	rep
	movsl
	popl	%edi
	popl	%esi
	cld
	popl	%ebp
	ret
END(bcopy)

/*
 * Note: memcpy does not support overlapping copies
 */
ENTRY(memcpy)
	pushl	%edi
	pushl	%esi
	movl	12(%esp),%edi
	movl	16(%esp),%esi
	movl	20(%esp),%ecx
	movl	%edi,%eax
	shrl	$2,%ecx				/* copy by 32-bit words */
	rep
	movsl
	movl	20(%esp),%ecx
	andl	$3,%ecx				/* any bytes left? */
	rep
	movsb
	popl	%esi
	popl	%edi
	ret
END(memcpy)

/*****************************************************************************/
/* copyout and fubyte family                                                 */
/*****************************************************************************/
/*
 * Access user memory from inside the kernel. These routines and possibly
 * the math- and DOS emulators should be the only places that do this.
 *
 * We have to access the memory with user's permissions, so use a segment
 * selector with RPL 3. For writes to user space we have to additionally
 * check the PTE for write permission, because the 386 does not check
 * write permissions when we are executing with EPL 0. The 486 does check
 * this if the WP bit is set in CR0, so we can use a simpler version here.
 *
 * These routines set curpcb->pcb_onfault for the time they execute. When a
 * protection violation occurs inside the functions, the trap handler
 * returns to *curpcb->pcb_onfault instead of the function.
 */

/*
 * copyout(from_kernel, to_user, len)  - MP SAFE
 */
ENTRY(copyout)
	movl	PCPU(CURPCB),%eax
	movl	$copyout_fault,PCB_ONFAULT(%eax)
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	movl	16(%esp),%esi
	movl	20(%esp),%edi
	movl	24(%esp),%ebx
	testl	%ebx,%ebx			/* anything to do? */
	jz	done_copyout

	/*
	 * Check explicitly for non-user addresses.  This check is essential
	 * because it prevents usermode from writing into the kernel.  We do
	 * not verify anywhere else that the user did not specify a rogue
	 * address.
	 */
	/*
	 * First, prevent address wrapping.
	 */
	movl	%edi,%eax
	addl	%ebx,%eax
	jc	copyout_fault
/*
 * XXX STOP USING VM_MAXUSER_ADDRESS.
 * It is an end address, not a max, so every time it is used correctly it
 * looks like there is an off by one error, and of course it caused an off
 * by one error in several places.
 */
	cmpl	$VM_MAXUSER_ADDRESS,%eax
	ja	copyout_fault

	/* bcopy(%esi, %edi, %ebx) */
	movl	%ebx,%ecx

	shrl	$2,%ecx
	rep
	movsl
	movb	%bl,%cl
	andb	$3,%cl
	rep
	movsb

done_copyout:
	popl	%ebx
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	movl	PCPU(CURPCB),%edx
	movl	%eax,PCB_ONFAULT(%edx)
	ret
END(copyout)

	ALIGN_TEXT
copyout_fault:
	popl	%ebx
	popl	%edi
	popl	%esi
	movl	PCPU(CURPCB),%edx
	movl	$0,PCB_ONFAULT(%edx)
	movl	$EFAULT,%eax
	ret

/*
 * copyin(from_user, to_kernel, len) - MP SAFE
 */
ENTRY(copyin)
	movl	PCPU(CURPCB),%eax
	movl	$copyin_fault,PCB_ONFAULT(%eax)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi			/* caddr_t from */
	movl	16(%esp),%edi			/* caddr_t to */
	movl	20(%esp),%ecx			/* size_t  len */

	/*
	 * make sure address is valid
	 */
	movl	%esi,%edx
	addl	%ecx,%edx
	jc	copyin_fault
	cmpl	$VM_MAXUSER_ADDRESS,%edx
	ja	copyin_fault

	movb	%cl,%al
	shrl	$2,%ecx				/* copy longword-wise */
	rep
	movsl
	movb	%al,%cl
	andb	$3,%cl				/* copy remaining bytes */
	rep
	movsb

	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	movl	PCPU(CURPCB),%edx
	movl	%eax,PCB_ONFAULT(%edx)
	ret
END(copyin)

	ALIGN_TEXT
copyin_fault:
	popl	%edi
	popl	%esi
	movl	PCPU(CURPCB),%edx
	movl	$0,PCB_ONFAULT(%edx)
	movl	$EFAULT,%eax
	ret

/*
 * casueword.  Compare and set user word.  Returns -1 on fault,
 * 0 on non-faulting access.  The current value is in *oldp.
 */
ALTENTRY(casueword32)
ENTRY(casueword)
	movl	PCPU(CURPCB),%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx			/* dst */
	movl	8(%esp),%eax			/* old */
	movl	16(%esp),%ecx			/* new */

	cmpl	$VM_MAXUSER_ADDRESS-4,%edx	/* verify address is valid */
	ja	fusufault

#ifdef SMP
	lock
#endif
	cmpxchgl %ecx,(%edx)			/* Compare and set. */

	/*
	 * The old value is in %eax.  If the store succeeded it will be the
	 * value we expected (old) from before the store, otherwise it will
	 * be the current value.
	 */

	movl	PCPU(CURPCB),%ecx
	movl	$0,PCB_ONFAULT(%ecx)
	movl	12(%esp),%edx			/* oldp */
	movl	%eax,(%edx)
	xorl	%eax,%eax
	ret
END(casueword32)
END(casueword)

/*
 * Fetch (load) a 32-bit word, a 16-bit word, or an 8-bit byte from user
 * memory.
 */

ALTENTRY(fueword32)
ENTRY(fueword)
	movl	PCPU(CURPCB),%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx			/* from */

	cmpl	$VM_MAXUSER_ADDRESS-4,%edx	/* verify address is valid */
	ja	fusufault

	movl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	movl	8(%esp),%edx
	movl	%eax,(%edx)
	xorl	%eax,%eax
	ret
END(fueword32)
END(fueword)

/*
 * fuswintr() and suswintr() are specialized variants of fuword16() and
 * suword16(), respectively.  They are called from the profiling code,
 * potentially at interrupt time.  If they fail, that's okay; good things
 * will happen later.  They always fail for now, until the trap code is
 * able to deal with this.
 */
ALTENTRY(suswintr)
ENTRY(fuswintr)
	movl	$-1,%eax
	ret
END(suswintr)
END(fuswintr)

ENTRY(fuword16)
	movl	PCPU(CURPCB),%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	fusufault

	movzwl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret
END(fuword16)

ENTRY(fubyte)
	movl	PCPU(CURPCB),%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

	cmpl	$VM_MAXUSER_ADDRESS-1,%edx
	ja	fusufault

	movzbl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret
END(fubyte)

	ALIGN_TEXT
fusufault:
	movl	PCPU(CURPCB),%ecx
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	decl	%eax
	ret

/*
 * Store a 32-bit word, a 16-bit word, or an 8-bit byte to user memory.
 * All these functions are MPSAFE.
 */

ALTENTRY(suword32)
ENTRY(suword)
	movl	PCPU(CURPCB),%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

	cmpl	$VM_MAXUSER_ADDRESS-4,%edx	/* verify address validity */
	ja	fusufault

	movl	8(%esp),%eax
	movl	%eax,(%edx)
	xorl	%eax,%eax
	movl	PCPU(CURPCB),%ecx
	movl	%eax,PCB_ONFAULT(%ecx)
	ret
END(suword32)
END(suword)

ENTRY(suword16)
	movl	PCPU(CURPCB),%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

	cmpl	$VM_MAXUSER_ADDRESS-2,%edx	/* verify address validity */
	ja	fusufault

	movw	8(%esp),%ax
	movw	%ax,(%edx)
	xorl	%eax,%eax
	movl	PCPU(CURPCB),%ecx		/* restore trashed register */
	movl	%eax,PCB_ONFAULT(%ecx)
	ret
END(suword16)

ENTRY(subyte)
	movl	PCPU(CURPCB),%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

	cmpl	$VM_MAXUSER_ADDRESS-1,%edx	/* verify address validity */
	ja	fusufault

	movb	8(%esp),%al
	movb	%al,(%edx)
	xorl	%eax,%eax
	movl	PCPU(CURPCB),%ecx		/* restore trashed register */
	movl	%eax,PCB_ONFAULT(%ecx)
	ret
END(subyte)

/*
 * copyinstr(from, to, maxlen, int *lencopied) - MP SAFE
 *
 *	copy a string from 'from' to 'to', stop when a 0 character is reached.
 *	return ENAMETOOLONG if string is longer than maxlen, and
 *	EFAULT on protection violations. If lencopied is non-zero,
 *	return the actual length in *lencopied.
 */
ENTRY(copyinstr)
	pushl	%esi
	pushl	%edi
	movl	PCPU(CURPCB),%ecx
	movl	$cpystrflt,PCB_ONFAULT(%ecx)

	movl	12(%esp),%esi			/* %esi = from */
	movl	16(%esp),%edi			/* %edi = to */
	movl	20(%esp),%edx			/* %edx = maxlen */

	movl	$VM_MAXUSER_ADDRESS,%eax

	/* make sure 'from' is within bounds */
	subl	%esi,%eax
	jbe	cpystrflt

	/* restrict maxlen to <= VM_MAXUSER_ADDRESS-from */
	cmpl	%edx,%eax
	jae	1f
	movl	%eax,%edx
	movl	%eax,20(%esp)
1:
	incl	%edx

2:
	decl	%edx
	jz	3f

	lodsb
	stosb
	orb	%al,%al
	jnz	2b

	/* Success -- 0 byte reached */
	decl	%edx
	xorl	%eax,%eax
	jmp	cpystrflt_x
3:
	/* edx is zero - return ENAMETOOLONG or EFAULT */
	cmpl	$VM_MAXUSER_ADDRESS,%esi
	jae	cpystrflt
4:
	movl	$ENAMETOOLONG,%eax
	jmp	cpystrflt_x

cpystrflt:
	movl	$EFAULT,%eax

cpystrflt_x:
	/* set *lencopied and return %eax */
	movl	PCPU(CURPCB),%ecx
	movl	$0,PCB_ONFAULT(%ecx)
	movl	20(%esp),%ecx
	subl	%edx,%ecx
	movl	24(%esp),%edx
	testl	%edx,%edx
	jz	1f
	movl	%ecx,(%edx)
1:
	popl	%edi
	popl	%esi
	ret
END(copyinstr)

/*
 * copystr(from, to, maxlen, int *lencopied) - MP SAFE
 */
ENTRY(copystr)
	pushl	%esi
	pushl	%edi

	movl	12(%esp),%esi			/* %esi = from */
	movl	16(%esp),%edi			/* %edi = to */
	movl	20(%esp),%edx			/* %edx = maxlen */
	incl	%edx
1:
	decl	%edx
	jz	4f
	lodsb
	stosb
	orb	%al,%al
	jnz	1b

	/* Success -- 0 byte reached */
	decl	%edx
	xorl	%eax,%eax
	jmp	6f
4:
	/* edx is zero -- return ENAMETOOLONG */
	movl	$ENAMETOOLONG,%eax

6:
	/* set *lencopied and return %eax */
	movl	20(%esp),%ecx
	subl	%edx,%ecx
	movl	24(%esp),%edx
	testl	%edx,%edx
	jz	7f
	movl	%ecx,(%edx)
7:
	popl	%edi
	popl	%esi
	ret
END(copystr)

ENTRY(bcmp)
	pushl	%edi
	pushl	%esi
	movl	12(%esp),%edi
	movl	16(%esp),%esi
	movl	20(%esp),%edx

	movl	%edx,%ecx
	shrl	$2,%ecx
	repe
	cmpsl
	jne	1f

	movl	%edx,%ecx
	andl	$3,%ecx
	repe
	cmpsb
1:
	setne	%al
	movsbl	%al,%eax
	popl	%esi
	popl	%edi
	ret
END(bcmp)

/*
 * Handling of special 386 registers and descriptor tables etc
 */
/* void lgdt(struct region_descriptor *rdp); */
ENTRY(lgdt)
	/* reload the descriptor table */
	movl	4(%esp),%eax
	lgdt	(%eax)

	/* flush the prefetch q */
	jmp	1f
	nop
1:
	/* reload "stale" selectors */
	movl	$KDSEL,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%gs
	movl	%eax,%ss
	movl	$KPSEL,%eax
	movl	%eax,%fs

	/* reload code selector by turning return into intersegmental return */
	movl	(%esp),%eax
	pushl	%eax
	movl	$KCSEL,4(%esp)
	MEXITCOUNT
	lret
END(lgdt)

/* ssdtosd(*ssdp,*sdp) */
ENTRY(ssdtosd)
	pushl	%ebx
	movl	8(%esp),%ecx
	movl	8(%ecx),%ebx
	shll	$16,%ebx
	movl	(%ecx),%edx
	roll	$16,%edx
	movb	%dh,%bl
	movb	%dl,%bh
	rorl	$8,%ebx
	movl	4(%ecx),%eax
	movw	%ax,%dx
	andl	$0xf0000,%eax
	orl	%eax,%ebx
	movl	12(%esp),%ecx
	movl	%edx,(%ecx)
	movl	%ebx,4(%ecx)
	popl	%ebx
	ret
END(ssdtosd)

/* void reset_dbregs() */
ENTRY(reset_dbregs)
	movl	$0,%eax
	movl	%eax,%dr7	/* disable all breakpoints first */
	movl	%eax,%dr0
	movl	%eax,%dr1
	movl	%eax,%dr2
	movl	%eax,%dr3
	movl	%eax,%dr6
	ret
END(reset_dbregs)

/*****************************************************************************/
/* setjump, longjump                                                         */
/*****************************************************************************/

ENTRY(setjmp)
	movl	4(%esp),%eax
	movl	%ebx,(%eax)			/* save ebx */
	movl	%esp,4(%eax)			/* save esp */
	movl	%ebp,8(%eax)			/* save ebp */
	movl	%esi,12(%eax)			/* save esi */
	movl	%edi,16(%eax)			/* save edi */
	movl	(%esp),%edx			/* get rta */
	movl	%edx,20(%eax)			/* save eip */
	xorl	%eax,%eax			/* return(0); */
	ret
END(setjmp)

ENTRY(longjmp)
	movl	4(%esp),%eax
	movl	(%eax),%ebx			/* restore ebx */
	movl	4(%eax),%esp			/* restore esp */
	movl	8(%eax),%ebp			/* restore ebp */
	movl	12(%eax),%esi			/* restore esi */
	movl	16(%eax),%edi			/* restore edi */
	movl	20(%eax),%edx			/* get rta */
	movl	%edx,(%esp)			/* put in return frame */
	xorl	%eax,%eax			/* return(1); */
	incl	%eax
	ret
END(longjmp)

/*
 * Support for reading MSRs in the safe manner.  (Instead of panic on #gp,
 * return an error.)
 */
ENTRY(rdmsr_safe)
/* int rdmsr_safe(u_int msr, uint64_t *data) */
	movl	PCPU(CURPCB),%ecx
	movl	$msr_onfault,PCB_ONFAULT(%ecx)

	movl	4(%esp),%ecx
	rdmsr
	movl	8(%esp),%ecx
	movl	%eax,(%ecx)
	movl	%edx,4(%ecx)
	xorl	%eax,%eax

	movl	PCPU(CURPCB),%ecx
	movl	%eax,PCB_ONFAULT(%ecx)

	ret

/*
 * Support for writing MSRs in the safe manner.  (Instead of panic on #gp,
 * return an error.)
 */
ENTRY(wrmsr_safe)
/* int wrmsr_safe(u_int msr, uint64_t data) */
	movl	PCPU(CURPCB),%ecx
	movl	$msr_onfault,PCB_ONFAULT(%ecx)

	movl	4(%esp),%ecx
	movl	8(%esp),%eax
	movl	12(%esp),%edx
	wrmsr
	xorl	%eax,%eax

	movl	PCPU(CURPCB),%ecx
	movl	%eax,PCB_ONFAULT(%ecx)

	ret

/*
 * MSR operations fault handler
 */
	ALIGN_TEXT
msr_onfault:
	movl	PCPU(CURPCB),%ecx
	movl	$0,PCB_ONFAULT(%ecx)
	movl	$EFAULT,%eax
	ret

ENTRY(handle_ibrs_entry)
	ret
END(handle_ibrs_entry)

ENTRY(handle_ibrs_exit)
	ret
END(handle_ibrs_exit)
