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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	$Id: support.s,v 1.41.2.2 1996/11/12 09:08:04 phk Exp $
 */

#include "opt_cpu.h"

#include <machine/asmacros.h>
#include <machine/cputypes.h>
#include <machine/specialreg.h>

#include "assym.s"

#define KDSEL		0x10			/* kernel data selector */
#define IDXSHIFT	10

	.data
	.globl	_bcopy_vector
_bcopy_vector:
	.long	_generic_bcopy
	.globl	_bzero
_bzero:
	.long	_generic_bzero
	.globl	_copyin_vector
_copyin_vector:
	.long	_generic_copyin
	.globl	_copyout_vector
_copyout_vector:
	.long	_generic_copyout
	.globl	_ovbcopy_vector
_ovbcopy_vector:
	.long	_generic_bcopy
kernel_fpu_lock:
	.byte	0xfe
	.space	3

	.text

/*
 * bcopy family
 * void bzero(void *buf, u_int len)
 */

ENTRY(generic_bzero)
	pushl	%edi
	movl	8(%esp),%edi
	movl	12(%esp),%ecx
	xorl	%eax,%eax
	shrl	$2,%ecx
	cld
	rep
	stosl
	movl	12(%esp),%ecx
	andl	$3,%ecx
	rep
	stosb
	popl	%edi
	ret

#if defined(I486_CPU)
ENTRY(i486_bzero)
	movl	4(%esp),%edx
	movl	8(%esp),%ecx
	xorl	%eax,%eax
/*
 * do 64 byte chunks first
 *
 * XXX this is probably over-unrolled at least for DX2's
 */
2:
	cmpl	$64,%ecx
	jb	3f
	movl	%eax,(%edx)
	movl	%eax,4(%edx)
	movl	%eax,8(%edx)
	movl	%eax,12(%edx)
	movl	%eax,16(%edx)
	movl	%eax,20(%edx)
	movl	%eax,24(%edx)
	movl	%eax,28(%edx)
	movl	%eax,32(%edx)
	movl	%eax,36(%edx)
	movl	%eax,40(%edx)
	movl	%eax,44(%edx)
	movl	%eax,48(%edx)
	movl	%eax,52(%edx)
	movl	%eax,56(%edx)
	movl	%eax,60(%edx)
	addl	$64,%edx
	subl	$64,%ecx
	jnz	2b
	ret

/*
 * do 16 byte chunks
 */
	SUPERALIGN_TEXT
3:
	cmpl	$16,%ecx
	jb	4f
	movl	%eax,(%edx)
	movl	%eax,4(%edx)
	movl	%eax,8(%edx)
	movl	%eax,12(%edx)
	addl	$16,%edx
	subl	$16,%ecx
	jnz	3b
	ret

/*
 * do 4 byte chunks
 */
	SUPERALIGN_TEXT
4:
	cmpl	$4,%ecx
	jb	5f
	movl	%eax,(%edx)
	addl	$4,%edx
	subl	$4,%ecx
	jnz	4b
	ret

/*
 * do 1 byte chunks
 * a jump table seems to be faster than a loop or more range reductions
 *
 * XXX need a const section for non-text
 */
	.data
jtab:
	.long	do0
	.long	do1
	.long	do2
	.long	do3

	.text
	SUPERALIGN_TEXT
5:
	jmp	jtab(,%ecx,4)

	SUPERALIGN_TEXT
do3:
	movw	%ax,(%edx)
	movb	%al,2(%edx)
	ret

	SUPERALIGN_TEXT
do2:
	movw	%ax,(%edx)
	ret

	SUPERALIGN_TEXT
do1:
	movb	%al,(%edx)
	ret

	SUPERALIGN_TEXT
do0:
	ret
#endif

#ifdef I586_CPU
ENTRY(i586_bzero)
	movl	4(%esp),%edx
	movl	8(%esp),%ecx

	/*
	 * The FPU register method is twice as fast as the integer register
	 * method unless the target is in the L1 cache and we pre-allocate a
	 * cache line for it (then the integer register method is 4-5 times
	 * faster).  However, we never pre-allocate cache lines, since that
	 * would make the integer method 25% or more slower for the common
	 * case when the target isn't in either the L1 cache or the L2 cache.
	 * Thus we normally use the FPU register method unless the overhead
	 * would be too large.
	 */
	cmpl	$256,%ecx	/* empirical; clts, fninit, smsw cost a lot */
	jb	intreg_i586_bzero

	/*
	 * The FPU registers may belong to an application or to fastmove()
	 * or to another invocation of bcopy() or ourself in a higher level
	 * interrupt or trap handler.  Preserving the registers is
	 * complicated since we avoid it if possible at all levels.  We
	 * want to localize the complications even when that increases them.
	 * Here the extra work involves preserving CR0_TS in TS.
	 * `npxproc != NULL' is supposed to be the condition that all the
	 * FPU resources belong to an application, but npxproc and CR0_TS
	 * aren't set atomically enough for this condition to work in
	 * interrupt handlers.
	 *
	 * Case 1: FPU registers belong to the application: we must preserve
	 * the registers if we use them, so we only use the FPU register
	 * method if the target size is large enough to amortize the extra
	 * overhead for preserving them.  CR0_TS must be preserved although
	 * it is very likely to end up as set.
	 *
	 * Case 2: FPU registers belong to fastmove(): fastmove() currently
	 * makes the registers look like they belong to an application so
	 * that cpu_switch() and savectx() don't have to know about it, so
	 * this case reduces to case 1.
	 *
	 * Case 3: FPU registers belong to the kernel: don't use the FPU
	 * register method.  This case is unlikely, and supporting it would
	 * be more complicated and might take too much stack.
	 *
	 * Case 4: FPU registers don't belong to anyone: the FPU registers
	 * don't need to be preserved, so we always use the FPU register
	 * method.  CR0_TS must be preserved although it is very likely to
	 * always end up as clear.
	 */
	cmpl	$0,_npxproc
	je	i586_bz1
	cmpl	$256+184,%ecx		/* empirical; not quite 2*108 more */
	jb	intreg_i586_bzero
	sarb	$1,kernel_fpu_lock
	jc	intreg_i586_bzero
	smsw	%ax
	clts
	subl	$108,%esp
	fnsave	0(%esp)
	jmp	i586_bz2

i586_bz1:
	sarb	$1,kernel_fpu_lock
	jc	intreg_i586_bzero
	smsw	%ax
	clts
	fninit				/* XXX should avoid needing this */
i586_bz2:
	fldz

	/*
	 * Align to an 8 byte boundary (misalignment in the main loop would
	 * cost a factor of >= 2).  Avoid jumps (at little cost if it is
	 * already aligned) by always zeroing 8 bytes and using the part up
	 * to the _next_ alignment position.
	 */
	fstl	0(%edx)
	addl	%edx,%ecx		/* part of %ecx -= new_%edx - %edx */
	addl	$8,%edx
	andl	$~7,%edx
	subl	%edx,%ecx

	/*
	 * Similarly align `len' to a multiple of 8.
	 */
	fstl	-8(%edx,%ecx)
	decl	%ecx
	andl	$~7,%ecx

	/*
	 * This wouldn't be any faster if it were unrolled, since the loop
	 * control instructions are much faster than the fstl and/or done
	 * in parallel with it so their overhead is insignificant.
	 */
fpureg_i586_bzero_loop:
	fstl	0(%edx)
	addl	$8,%edx
	subl	$8,%ecx
	cmpl	$8,%ecx
	jae	fpureg_i586_bzero_loop

	cmpl	$0,_npxproc
	je	i586_bz3
	frstor	0(%esp)
	addl	$108,%esp
	lmsw	%ax
	movb	$0xfe,kernel_fpu_lock
	ret

i586_bz3:
	fstpl	%st(0)
	lmsw	%ax
	movb	$0xfe,kernel_fpu_lock
	ret

intreg_i586_bzero:
	/*
	 * `rep stos' seems to be the best method in practice for small
	 * counts.  Fancy methods usually take too long to start up due
	 * to cache and BTB misses.
	 */
	pushl	%edi
	movl	%edx,%edi
	xorl	%eax,%eax
	shrl	$2,%ecx
	cld
	rep
	stosl
	movl	12(%esp),%ecx
	andl	$3,%ecx
	jne	1f
	popl	%edi
	ret

1:
	rep
	stosb
	popl	%edi
	ret
#endif /* I586_CPU */

/* fillw(pat, base, cnt) */
ENTRY(fillw)
	pushl	%edi
	movl	8(%esp),%eax
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	cld
	rep
	stosw
	popl	%edi
	ret

ENTRY(bcopyb)
bcopyb:
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax			/* overlapping && src < dst? */
	jb	1f
	cld					/* nope, copy forwards */
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

ENTRY(bcopy)
	MEXITCOUNT
	jmp	*_bcopy_vector

ENTRY(ovbcopy)
	MEXITCOUNT
	jmp	*_ovbcopy_vector

/*
 * generic_bcopy(src, dst, cnt)
 *  ws@tools.de     (Wolfgang Solfrank, TooLs GmbH) +49-228-985800
 */
ENTRY(generic_bcopy)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx

	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax			/* overlapping && src < dst? */
	jb	1f

	shrl	$2,%ecx				/* copy by 32-bit words */
	cld					/* nope, copy forwards */
	rep
	movsl
	movl	20(%esp),%ecx
	andl	$3,%ecx				/* any bytes left? */
	rep
	movsb
	popl	%edi
	popl	%esi
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
	movl	20(%esp),%ecx			/* copy remainder by 32-bit words */
	shrl	$2,%ecx
	subl	$3,%esi
	subl	$3,%edi
	rep
	movsl
	popl	%edi
	popl	%esi
	cld
	ret

#ifdef I586_CPU
ENTRY(i586_bcopy)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx

	movl	%edi,%eax
	subl	%esi,%eax
	cmpl	%ecx,%eax			/* overlapping && src < dst? */
	jb	1f

	cmpl	$1024,%ecx
	jb	small_i586_bcopy

	sarb	$1,kernel_fpu_lock
	jc	small_i586_bcopy
	cmpl	$0,_npxproc
	je	i586_bc1
	smsw	%dx
	clts
	subl	$108,%esp
	fnsave	0(%esp)
	jmp	4f

i586_bc1:
	smsw	%dx
	clts
	fninit				/* XXX should avoid needing this */

	ALIGN_TEXT
4:
	pushl	%ecx
#define	DCACHE_SIZE	8192
	cmpl	$(DCACHE_SIZE-512)/2,%ecx
	jbe	2f
	movl	$(DCACHE_SIZE-512)/2,%ecx
2:
	subl	%ecx,0(%esp)
	cmpl	$256,%ecx
	jb	5f			/* XXX should prefetch if %ecx >= 32 */
	pushl	%esi
	pushl	%ecx
	ALIGN_TEXT
3:
	movl	0(%esi),%eax
	movl	32(%esi),%eax
	movl	64(%esi),%eax
	movl	96(%esi),%eax
	movl	128(%esi),%eax
	movl	160(%esi),%eax
	movl	192(%esi),%eax
	movl	224(%esi),%eax
	addl	$256,%esi
	subl	$256,%ecx
	cmpl	$256,%ecx
	jae	3b
	popl	%ecx
	popl	%esi
5:
	ALIGN_TEXT
large_i586_bcopy_loop:
	fildq	0(%esi)
	fildq	8(%esi)
	fildq	16(%esi)
	fildq	24(%esi)
	fildq	32(%esi)
	fildq	40(%esi)
	fildq	48(%esi)
	fildq	56(%esi)
	fistpq	56(%edi)
	fistpq	48(%edi)
	fistpq	40(%edi)
	fistpq	32(%edi)
	fistpq	24(%edi)
	fistpq	16(%edi)
	fistpq	8(%edi)
	fistpq	0(%edi)
	addl	$64,%esi
	addl	$64,%edi
	subl	$64,%ecx
	cmpl	$64,%ecx
	jae	large_i586_bcopy_loop
	popl	%eax
	addl	%eax,%ecx
	cmpl	$64,%ecx
	jae	4b

	cmpl	$0,_npxproc
	je	i586_bc2
	frstor	0(%esp)
	addl	$108,%esp
i586_bc2:
	lmsw	%dx
	movb	$0xfe,kernel_fpu_lock

/*
 * This is a duplicate of the main part of generic_bcopy.  See the comments
 * there.  Jumping into generic_bcopy would cost a whole 0-1 cycles and
 * would mess up high resolution profiling.
 */
	ALIGN_TEXT
small_i586_bcopy:
	shrl	$2,%ecx
	cld
	rep
	movsl
	movl	20(%esp),%ecx
	andl	$3,%ecx
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:
	addl	%ecx,%edi
	addl	%ecx,%esi
	decl	%edi
	decl	%esi
	andl	$3,%ecx
	std
	rep
	movsb
	movl	20(%esp),%ecx
	shrl	$2,%ecx
	subl	$3,%esi
	subl	$3,%edi
	rep
	movsl
	popl	%edi
	popl	%esi
	cld
	ret
#endif /* I586_CPU */

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
	cld					/* nope, copy forwards */
	rep
	movsl
	movl	20(%esp),%ecx
	andl	$3,%ecx				/* any bytes left? */
	rep
	movsb
	popl	%esi
	popl	%edi
	ret


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
 * These routines set curpcb->onfault for the time they execute. When a
 * protection violation occurs inside the functions, the trap handler
 * returns to *curpcb->onfault instead of the function.
 */

/* copyout(from_kernel, to_user, len) */
ENTRY(copyout)
	MEXITCOUNT
	jmp	*_copyout_vector

ENTRY(generic_copyout)
	movl	_curpcb,%eax
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
	 * Check explicitly for non-user addresses.  If 486 write protection
	 * is being used, this check is essential because we are in kernel
	 * mode so the h/w does not provide any protection against writing
	 * kernel addresses.
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

#if defined(I386_CPU)

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	3f
#endif
/*
 * We have to check each PTE for user write permission.
 * The checking may cause a page fault, so it is important to set
 * up everything for return via copyout_fault before here.
 */
	/* compute number of pages */
	movl	%edi,%ecx
	andl	$PAGE_MASK,%ecx
	addl	%ebx,%ecx
	decl	%ecx
	shrl	$IDXSHIFT+2,%ecx
	incl	%ecx

	/* compute PTE offset for start address */
	movl	%edi,%edx
	shrl	$IDXSHIFT,%edx
	andb	$0xfc,%dl

1:	/* check PTE for each page */
	movb	_PTmap(%edx),%al
	andb	$0x07,%al			/* Pages must be VALID + USERACC + WRITABLE */
	cmpb	$0x07,%al
	je	2f

	/* simulate a trap */
	pushl	%edx
	pushl	%ecx
	shll	$IDXSHIFT,%edx
	pushl	%edx
	call	_trapwrite			/* trapwrite(addr) */
	popl	%edx
	popl	%ecx
	popl	%edx

	testl	%eax,%eax			/* if not ok, return EFAULT */
	jnz	copyout_fault

2:
	addl	$4,%edx
	decl	%ecx
	jnz	1b				/* check next page */
#endif /* I386_CPU */

	/* bcopy(%esi, %edi, %ebx) */
3:
	movl	%ebx,%ecx

#ifdef I586_CPU
	ALIGN_TEXT
slow_copyout:
#endif
	shrl	$2,%ecx
	cld
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
	movl	_curpcb,%edx
	movl	%eax,PCB_ONFAULT(%edx)
	ret

	ALIGN_TEXT
copyout_fault:
	popl	%ebx
	popl	%edi
	popl	%esi
	movl	_curpcb,%edx
	movl	$0,PCB_ONFAULT(%edx)
	movl	$EFAULT,%eax
	ret

#ifdef I586_CPU
ENTRY(i586_copyout)
	/*
	 * Duplicated from generic_copyout.  Could be done a bit better.
	 */
	movl	_curpcb,%eax
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
	 * Check explicitly for non-user addresses.  If 486 write protection
	 * is being used, this check is essential because we are in kernel
	 * mode so the h/w does not provide any protection against writing
	 * kernel addresses.
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
3:
	movl	%ebx,%ecx
	/*
	 * End of duplicated code.
	 */

	cmpl	$1024,%ecx
	jb	slow_copyout

	pushl	%ecx
	call	_fastmove
	addl	$4,%esp
	jmp	done_copyout
#endif /* I586_CPU */

/* copyin(from_user, to_kernel, len) */
ENTRY(copyin)
	MEXITCOUNT
	jmp	*_copyin_vector

ENTRY(generic_copyin)
	movl	_curpcb,%eax
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

#ifdef I586_CPU
	ALIGN_TEXT
slow_copyin:
#endif
	movb	%cl,%al
	shrl	$2,%ecx				/* copy longword-wise */
	cld
	rep
	movsl
	movb	%al,%cl
	andb	$3,%cl				/* copy remaining bytes */
	rep
	movsb

#if defined(I586_CPU)
	ALIGN_TEXT
done_copyin:
#endif /* I586_CPU */
	popl	%edi
	popl	%esi
	xorl	%eax,%eax
	movl	_curpcb,%edx
	movl	%eax,PCB_ONFAULT(%edx)
	ret

	ALIGN_TEXT
copyin_fault:
	popl	%edi
	popl	%esi
	movl	_curpcb,%edx
	movl	$0,PCB_ONFAULT(%edx)
	movl	$EFAULT,%eax
	ret

#ifdef I586_CPU
ENTRY(i586_copyin)
	/*
	 * Duplicated from generic_copyin.  Could be done a bit better.
	 */
	movl	_curpcb,%eax
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
	/*
	 * End of duplicated code.
	 */

	cmpl	$1024,%ecx
	jb	slow_copyin

	pushl	%ecx
	call	_fastmove
	addl	$4,%esp
	jmp	done_copyin
#endif /* I586_CPU */

#if defined(I586_CPU)
/* fastmove(src, dst, len)
	src in %esi
	dst in %edi
	len in %ecx		XXX changed to on stack for profiling
	uses %eax and %edx for tmp. storage
 */
/* XXX use ENTRY() to get profiling.  fastmove() is actually a non-entry. */
ENTRY(fastmove)
	movl	4(%esp),%ecx
	cmpl	$63,%ecx
	jbe	fastmove_tail

	testl	$7,%esi	/* check if src addr is multiple of 8 */
	jnz	fastmove_tail

	testl	$7,%edi	/* check if dst addr is multiple of 8 */
	jnz	fastmove_tail

	pushl	%ebp
	movl	%esp,%ebp
	subl	$PCB_SAVEFPU_SIZE,%esp

/* if (npxproc != NULL) { */
	cmpl	$0,_npxproc
	je	6f
/*    fnsave(&curpcb->pcb_savefpu); */
	movl	_curpcb,%eax
	fnsave	PCB_SAVEFPU(%eax)
/*   npxproc = NULL; */
	movl	$0,_npxproc
/* } */
6:
/* now we own the FPU. */

/*
 * The process' FP state is saved in the pcb, but if we get
 * switched, the cpu_switch() will store our FP state in the
 * pcb.  It should be possible to avoid all the copying for
 * this, e.g., by setting a flag to tell cpu_switch() to
 * save the state somewhere else.
 */
/* tmp = curpcb->pcb_savefpu; */
	pushl	%edi
	pushl	%esi
	pushl	%ecx
	leal	-PCB_SAVEFPU_SIZE(%ebp),%edi
	movl	_curpcb,%esi
	addl	$PCB_SAVEFPU,%esi
	cld
	movl	$PCB_SAVEFPU_SIZE>>2,%ecx
	rep
	movsl
	popl	%ecx
	popl	%esi
	popl	%edi
/* stop_emulating(); */
	clts
/* npxproc = curproc; */
	movl	_curproc,%eax
	movl	%eax,_npxproc
4:
	pushl	%ecx
	cmpl	$1792,%ecx
	jbe	2f
	movl	$1792,%ecx
2:
	subl	%ecx,0(%esp)
	cmpl	$256,%ecx
	jb	5f
	pushl	%esi
	pushl	%ecx
	ALIGN_TEXT
3:
	movl	0(%esi),%eax
	movl	32(%esi),%eax
	movl	64(%esi),%eax
	movl	96(%esi),%eax
	movl	128(%esi),%eax
	movl	160(%esi),%eax
	movl	192(%esi),%eax
	movl	224(%esi),%eax
	addl	$256,%esi
	subl	$256,%ecx
	cmpl	$256,%ecx
	jae	3b
	popl	%ecx
	popl	%esi
5:
	ALIGN_TEXT
fastmove_loop:
	fildq	0(%esi)
	fildq	8(%esi)
	fildq	16(%esi)
	fildq	24(%esi)
	fildq	32(%esi)
	fildq	40(%esi)
	fildq	48(%esi)
	fildq	56(%esi)
	fistpq	56(%edi)
	fistpq	48(%edi)
	fistpq	40(%edi)
	fistpq	32(%edi)
	fistpq	24(%edi)
	fistpq	16(%edi)
	fistpq	8(%edi)
	fistpq	0(%edi)
	addl	$-64,%ecx
	addl	$64,%esi
	addl	$64,%edi
	cmpl	$63,%ecx
	ja	fastmove_loop
	popl	%eax
	addl	%eax,%ecx
	cmpl	$64,%ecx
	jae	4b
	
/* curpcb->pcb_savefpu = tmp; */
	pushl	%edi
	pushl	%esi
	pushl	%ecx
	movl	_curpcb,%edi
	addl	$PCB_SAVEFPU,%edi
	leal	-PCB_SAVEFPU_SIZE(%ebp),%esi
	cld
	movl	$PCB_SAVEFPU_SIZE>>2,%ecx
	rep
	movsl
	popl	%ecx
	popl	%esi
	popl	%edi

/* start_emulating(); */
	smsw	%ax
	orb	$CR0_TS,%al
	lmsw	%ax
/* npxproc = NULL; */
	movl	$0,_npxproc
	movl	%ebp,%esp
	popl	%ebp
	
	ALIGN_TEXT
fastmove_tail:
	movb	%cl,%al
	shrl	$2,%ecx				/* copy longword-wise */
	cld
	rep
	movsl
	movb	%al,%cl
	andb	$3,%cl				/* copy remaining bytes */
	rep
	movsb

	ret
#endif /* I586_CPU */

/*
 * fu{byte,sword,word} : fetch a byte (sword, word) from user memory
 */
ENTRY(fuword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx			/* from */

	cmpl	$VM_MAXUSER_ADDRESS-4,%edx	/* verify address is valid */
	ja	fusufault

	movl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

/*
 * These two routines are called from the profiling code, potentially
 * at interrupt time. If they fail, that's okay, good things will
 * happen later. Fail all the time for now - until the trap code is
 * able to deal with this.
 */
ALTENTRY(suswintr)
ENTRY(fuswintr)
	movl	$-1,%eax
	ret

ENTRY(fusword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

	cmpl	$VM_MAXUSER_ADDRESS-2,%edx
	ja	fusufault

	movzwl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

ENTRY(fubyte)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

	cmpl	$VM_MAXUSER_ADDRESS-1,%edx
	ja	fusufault

	movzbl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

	ALIGN_TEXT
fusufault:
	movl	_curpcb,%ecx
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	decl	%eax
	ret

/*
 * su{byte,sword,word}: write a byte (word, longword) to user memory
 */
ENTRY(suword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

#if defined(I386_CPU)

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	2f				/* we only have to set the right segment selector */
#endif /* I486_CPU || I586_CPU || I686_CPU */

	/* XXX - page boundary crossing is still not handled */
	movl	%edx,%eax
	shrl	$IDXSHIFT,%edx
	andb	$0xfc,%dl
	movb	_PTmap(%edx),%dl
	andb	$0x7,%dl			/* must be VALID + USERACC + WRITE */
	cmpb	$0x7,%dl
	je	1f

	/* simulate a trap */
	pushl	%eax
	call	_trapwrite
	popl	%edx				/* remove junk parameter from stack */
	movl	_curpcb,%ecx			/* restore trashed register */
	testl	%eax,%eax
	jnz	fusufault
1:
	movl	4(%esp),%edx
#endif

2:	
	cmpl	$VM_MAXUSER_ADDRESS-4,%edx	/* verify address validity */
	ja	fusufault

	movl	8(%esp),%eax
	movl	%eax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ENTRY(susword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

#if defined(I386_CPU)

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	2f
#endif /* I486_CPU || I586_CPU || I686_CPU */

	/* XXX - page boundary crossing is still not handled */
	movl	%edx,%eax
	shrl	$IDXSHIFT,%edx
	andb	$0xfc,%dl
	movb	_PTmap(%edx),%dl
	andb	$0x7,%dl			/* must be VALID + USERACC + WRITE */
	cmpb	$0x7,%dl
	je	1f

	/* simulate a trap */
	pushl	%eax
	call	_trapwrite
	popl	%edx				/* remove junk parameter from stack */
	movl	_curpcb,%ecx			/* restore trashed register */
	testl	%eax,%eax
	jnz	fusufault
1:
	movl	4(%esp),%edx
#endif

2:
	cmpl	$VM_MAXUSER_ADDRESS-2,%edx	/* verify address validity */
	ja	fusufault

	movw	8(%esp),%ax
	movw	%ax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ALTENTRY(suibyte)
ENTRY(subyte)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx

#if defined(I386_CPU)

#if defined(I486_CPU) || defined(I586_CPU) || defined(I686_CPU)
	cmpl	$CPUCLASS_386,_cpu_class
	jne	2f
#endif /* I486_CPU || I586_CPU || I686_CPU */

	movl	%edx,%eax
	shrl	$IDXSHIFT,%edx
	andb	$0xfc,%dl
	movb	_PTmap(%edx),%dl
	andb	$0x7,%dl			/* must be VALID + USERACC + WRITE */
	cmpb	$0x7,%dl
	je	1f

	/* simulate a trap */
	pushl	%eax
	call	_trapwrite
	popl	%edx				/* remove junk parameter from stack */
	movl	_curpcb,%ecx			/* restore trashed register */
	testl	%eax,%eax
	jnz	fusufault
1:
	movl	4(%esp),%edx
#endif

2:
	cmpl	$VM_MAXUSER_ADDRESS-1,%edx	/* verify address validity */
	ja	fusufault

	movb	8(%esp),%al
	movb	%al,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

/*
 * copyinstr(from, to, maxlen, int *lencopied)
 *	copy a string from from to to, stop when a 0 character is reached.
 *	return ENAMETOOLONG if string is longer than maxlen, and
 *	EFAULT on protection violations. If lencopied is non-zero,
 *	return the actual length in *lencopied.
 */
ENTRY(copyinstr)
	pushl	%esi
	pushl	%edi
	movl	_curpcb,%ecx
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
	cld

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
	movl	_curpcb,%ecx
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


/*
 * copystr(from, to, maxlen, int *lencopied)
 */
ENTRY(copystr)
	pushl	%esi
	pushl	%edi

	movl	12(%esp),%esi			/* %esi = from */
	movl	16(%esp),%edi			/* %edi = to */
	movl	20(%esp),%edx			/* %edx = maxlen */
	incl	%edx
	cld
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

ENTRY(bcmp)
	pushl	%edi
	pushl	%esi
	movl	12(%esp),%edi
	movl	16(%esp),%esi
	movl	20(%esp),%edx
	xorl	%eax,%eax

	movl	%edx,%ecx
	shrl	$2,%ecx
	cld					/* compare forwards */
	repe
	cmpsl
	jne	1f

	movl	%edx,%ecx
	andl	$3,%ecx
	repe
	cmpsb
	je	2f
1:
	incl	%eax
2:
	popl	%esi
	popl	%edi
	ret


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
	movl	%ax,%ds
	movl	%ax,%es
	movl	%ax,%ss

	/* reload code selector by turning return into intersegmental return */
	movl	(%esp),%eax
	pushl	%eax
#	movl	$KCSEL,4(%esp)
	movl	$8,4(%esp)
	lret

/*
 * void lidt(struct region_descriptor *rdp);
 */
ENTRY(lidt)
	movl	4(%esp),%eax
	lidt	(%eax)
	ret

/*
 * void lldt(u_short sel)
 */
ENTRY(lldt)
	lldt	4(%esp)
	ret

/*
 * void ltr(u_short sel)
 */
ENTRY(ltr)
	ltr	4(%esp)
	ret

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

/* load_cr0(cr0) */
ENTRY(load_cr0)
	movl	4(%esp),%eax
	movl	%eax,%cr0
	ret

/* rcr0() */
ENTRY(rcr0)
	movl	%cr0,%eax
	ret

/* rcr3() */
ENTRY(rcr3)
	movl	%cr3,%eax
	ret

/* void load_cr3(caddr_t cr3) */
ENTRY(load_cr3)
	movl	4(%esp),%eax
	movl	%eax,%cr3
	ret


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

/*
 * Here for doing BB-profiling (gcc -a).
 * We rely on the "bbset" instead, but need a dummy function.
 */
NON_GPROF_ENTRY(__bb_init_func)
	movl	4(%esp),%eax
	movl	$1,(%eax)
	.byte	0xc3				/* avoid macro for `ret' */
