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
 *	$Id: support.s,v 1.2 1994/01/14 16:23:39 davidg Exp $
 */

#include "assym.s"				/* system definitions */
#include "errno.h"				/* error return codes */
#include "machine/asmacros.h"			/* miscellaneous asm macros */

#define KDSEL		0x10			/* kernel data selector */
#define IDXSHIFT	10

/*
 * Support routines for GCC, general C-callable functions
 */
ENTRY(__udivsi3)
	movl 4(%esp),%eax
	xorl %edx,%edx
	divl 8(%esp)
	ret

ENTRY(__divsi3)
	movl 4(%esp),%eax
	cltd
	idivl 8(%esp)
	ret

	/*
	 * I/O bus instructions via C
	 */
ENTRY(inb)					/* val = inb(port) */
	movl	4(%esp),%edx
	subl	%eax,%eax
	NOP
	inb	%dx,%al
	ret

ENTRY(inw)					/* val = inw(port) */
	movl	4(%esp),%edx
	subl	%eax,%eax
	NOP
	inw	%dx,%ax
	ret

ENTRY(insb)					/* insb(port, addr, cnt) */
	pushl	%edi
	movw	8(%esp),%dx
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	cld
	NOP
	rep
	insb
	NOP
	movl	%edi,%eax
	popl	%edi
	ret

ENTRY(insw)					/* insw(port, addr, cnt) */
	pushl	%edi
	movw	8(%esp),%dx
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	cld
	NOP
	rep
	insw
	NOP
	movl	%edi,%eax
	popl	%edi
	ret

ENTRY(rtcin)					/* rtcin(val) */
	movl	4(%esp),%eax
	outb	%al,$0x70
	NOP
	xorl	%eax,%eax
	inb	$0x71,%al
	ret

ENTRY(outb)					/* outb(port, val) */
	movl	4(%esp),%edx
	NOP
	movl	8(%esp),%eax
	outb	%al,%dx
	NOP
	ret

ENTRY(outw)					/* outw(port, val) */
	movl	4(%esp),%edx
	NOP
	movl	8(%esp),%eax
	outw	%ax,%dx
	NOP
	ret

ENTRY(outsb)					/* outsb(port, addr, cnt) */
	pushl	%esi
	movw	8(%esp),%dx
	movl	12(%esp),%esi
	movl	16(%esp),%ecx
	cld
	NOP
	rep
	outsb
	NOP
	movl	%esi,%eax
	popl	%esi
	ret

ENTRY(outsw)					/* outsw(port, addr, cnt) */
	pushl	%esi
	movw	8(%esp),%dx
	movl	12(%esp),%esi
	movl	16(%esp),%ecx
	cld
	NOP
	rep
	outsw
	NOP
	movl	%esi,%eax
	popl	%esi
	ret

/*
 * bcopy family
 */
/* void bzero(void *base, u_int cnt) */
ENTRY(bzero)
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

/* filli(pat, base, cnt) */
ENTRY(filli)
filli:
	pushl	%edi
	movl	8(%esp),%eax
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	cld
	rep
	stosl
	popl	%edi
	ret

ENTRY(bcopyb)
bcopyb:
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	cmpl	%esi,%edi			/* potentially overlapping? */
	jnb	1f
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
	std
	decl	%edi
	decl	%esi
	rep
	movsb
	popl	%edi
	popl	%esi
	cld
	ret

ENTRY(bcopyw)
bcopyw:
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	cmpl	%esi,%edi			/* potentially overlapping? */
	jnb	1f
	cld					/* nope, copy forwards */
	shrl	$1,%ecx				/* copy by 16-bit words */
	rep
	movsw
	adc	%ecx,%ecx			/* any bytes left? */
	rep
	movsb
	popl	%edi
	popl	%esi
	ret

	ALIGN_TEXT
1:
	addl	%ecx,%edi			/* copy backwards */
	addl	%ecx,%esi
	std
	andl	$1,%ecx				/* any fractional bytes? */
	decl	%edi
	decl	%esi
	rep
	movsb
	movl	20(%esp),%ecx			/* copy remainder by 16-bit words */
	shrl	$1,%ecx
	decl	%esi
	decl	%edi
	rep
	movsw
	popl	%edi
	popl	%esi
	cld
	ret

ENTRY(bcopyx)
	movl	16(%esp),%eax
	cmpl	$2,%eax
	je	bcopyw				/* not _bcopyw, to avoid multiple mcounts */
	cmpl	$4,%eax
	je	bcopy
	jmp	bcopyb

/*
 * (ov)bcopy(src, dst, cnt)
 *  ws@tools.de     (Wolfgang Solfrank, TooLs GmbH) +49-228-985800
 */
ALTENTRY(ovbcopy)
ENTRY(bcopy)
bcopy:
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi
	movl	16(%esp),%edi
	movl	20(%esp),%ecx
	cmpl	%esi,%edi			/* potentially overlapping? */
	jnb	1f
	cld					/* nope, copy forwards */
	shrl	$2,%ecx				/* copy by 32-bit words */
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
	std
	andl	$3,%ecx				/* any fractional bytes? */
	decl	%edi
	decl	%esi
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

ALTENTRY(ntohl)
ENTRY(htonl)
	movl	4(%esp),%eax
#ifdef i486
/* XXX */
/* Since Gas 1.38 does not grok bswap this has been coded as the
 * equivalent bytes.  This can be changed back to bswap when we
 * upgrade to a newer version of Gas */
	/* bswap	%eax */
	.byte	0x0f
	.byte	0xc8
#else
	xchgb	%al,%ah
	roll	$16,%eax
	xchgb	%al,%ah
#endif
	ret

ALTENTRY(ntohs)
ENTRY(htons)
	movzwl	4(%esp),%eax
	xchgb	%al,%ah
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


ENTRY(copyout)					/* copyout(from_kernel, to_user, len) */
	movl	_curpcb,%eax
	movl	$copyout_fault,PCB_ONFAULT(%eax)
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	movl	16(%esp),%esi
	movl	20(%esp),%edi
	movl	24(%esp),%ebx
	orl	%ebx,%ebx			/* anything to do? */
	jz	done_copyout

	/*
	 * Check explicitly for non-user addresses.  If 486 write protection
	 * is being used, this check is essential because we are in kernel
	 * mode so the h/w does not provide any protection against writing
	 * kernel addresses.
	 *
	 * Otherwise, it saves having to load and restore %es to get the
	 * usual segment-based protection (the destination segment for movs
	 * is always %es).  The other explicit checks for user-writablility
	 * are not quite sufficient.  They fail for the user area because
	 * we mapped the user area read/write to avoid having an #ifdef in
	 * vm_machdep.c.  They fail for user PTEs and/or PTDs!  (107
	 * addresses including 0xff800000 and 0xfc000000).  I'm not sure if
	 * this can be fixed.  Marking the PTEs supervisor mode and the
	 * PDE's user mode would almost work, but there may be a problem
	 * with the self-referential PDE.
	 */
	movl	%edi,%eax
	addl	%ebx,%eax
	jc	copyout_fault
	cmpl	$VM_MAXUSER_ADDRESS,%eax
	ja	copyout_fault

#ifndef USE_486_WRITE_PROTECT
/*
 * We have to check each PTE for user write permission.
 * The checking may cause a page fault, so it is important to set
 * up everything for return via copyout_fault before here.
 */
	/* compute number of pages */
	movl	%edi,%ecx
	andl	$NBPG-1,%ecx
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

	orl	%eax,%eax			/* if not ok, return EFAULT */
	jnz	copyout_fault

2:
	addl	$4,%edx
	decl	%ecx
	jnz	1b				/* check next page */
#endif /* ndef USE_486_WRITE_PROTECT */

	/* bcopy(%esi, %edi, %ebx) */
	cld
	movl	%ebx,%ecx
	shrl	$2,%ecx
	rep
	movsl
	movb	%bl,%cl
	andb	$3,%cl				/* XXX can we trust the rest of %ecx on clones? */
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

/* copyin(from_user, to_kernel, len) */
ENTRY(copyin)
	movl	_curpcb,%eax
	movl	$copyin_fault,PCB_ONFAULT(%eax)
	pushl	%esi
	pushl	%edi
	movl	12(%esp),%esi			/* caddr_t from */
	movl	16(%esp),%edi			/* caddr_t to */
	movl	20(%esp),%ecx			/* size_t  len */

	movb	%cl,%al
	shrl	$2,%ecx				/* copy longword-wise */
	cld
	gs
	rep
	movsl
	movb	%al,%cl
	andb	$3,%cl				/* copy remaining bytes */
	gs
	rep
	movsb

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

/*
 * fu{byte,sword,word} : fetch a byte(sword, word) from user memory
 */
ALTENTRY(fuiword)
ENTRY(fuword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	gs
	movl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

ENTRY(fusword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	gs
	movzwl	(%edx),%eax
	movl	$0,PCB_ONFAULT(%ecx)
	ret

ALTENTRY(fuibyte)
ENTRY(fubyte)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	gs
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
 * su{byte,sword,word}: write a byte(word, longword) to user memory
 */
#ifdef USE_486_WRITE_PROTECT
/*
 * we only have to set the right segment selector.
 */
ALTENTRY(suiword)
ENTRY(suword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	gs
	movl	%eax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ENTRY(susword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	movw	8(%esp),%ax
	gs
	movw	%ax,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ALTENTRY(suibyte)
ENTRY(subyte)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
	movb	8(%esp),%al
	gs
	movb	%al,(%edx)
	xorl	%eax,%eax
	movl	%eax,PCB_ONFAULT(%ecx)
	ret


#else /* USE_486_WRITE_PROTECT */
/*
 * here starts the trouble again: check PTE, twice if word crosses
 * a page boundary.
 */
/* XXX - page boundary crossing is not handled yet */

ALTENTRY(suibyte)
ENTRY(subyte)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
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
	popl	%edx
	orl	%eax,%eax
	jnz	fusufault
1:
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	gs
	movb	%al,(%edx)
	xorl	%eax,%eax
	movl	_curpcb,%ecx
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ENTRY(susword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
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
	popl	%edx
	orl	%eax,%eax
	jnz	fusufault
1:
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	gs
	movw	%ax,(%edx)
	xorl	%eax,%eax
	movl	_curpcb,%ecx
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

ALTENTRY(suiword)
ENTRY(suword)
	movl	_curpcb,%ecx
	movl	$fusufault,PCB_ONFAULT(%ecx)
	movl	4(%esp),%edx
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
	popl	%edx
	orl	%eax,%eax
	jnz	fusufault
1:
	movl	4(%esp),%edx
	movl	8(%esp),%eax
	gs
	movl	%eax,0(%edx)
	xorl	%eax,%eax
	movl	_curpcb,%ecx
	movl	%eax,PCB_ONFAULT(%ecx)
	ret

#endif /* USE_486_WRITE_PROTECT */

/*
 * copyoutstr(from, to, maxlen, int *lencopied)
 *	copy a string from from to to, stop when a 0 character is reached.
 *	return ENAMETOOLONG if string is longer than maxlen, and
 *	EFAULT on protection violations. If lencopied is non-zero,
 *	return the actual length in *lencopied.
 */
#ifdef USE_486_WRITE_PROTECT

ENTRY(copyoutstr)
	pushl	%esi
	pushl	%edi
	movl	_curpcb,%ecx
	movl	$cpystrflt,PCB_ONFAULT(%ecx)

	movl	12(%esp),%esi			/* %esi = from */
	movl	16(%esp),%edi			/* %edi = to */
	movl	20(%esp),%edx			/* %edx = maxlen */
	incl	%edx

1:
	decl	%edx
	jz	4f
	/*
	 * gs override doesn't work for stosb.  Use the same explicit check
	 * as in copyout().  It's much slower now because it is per-char.
	 * XXX - however, it would be faster to rewrite this function to use
	 * strlen() and copyout().
	 */
	cmpl	$VM_MAXUSER_ADDRESS,%edi
	jae	cpystrflt
	lodsb
	gs
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
	jmp	6f

#else	/* ndef USE_486_WRITE_PROTECT */

ENTRY(copyoutstr)
	pushl	%esi
	pushl	%edi
	movl	_curpcb,%ecx
	movl	$cpystrflt,PCB_ONFAULT(%ecx)

	movl	12(%esp),%esi			/* %esi = from */
	movl	16(%esp),%edi			/* %edi = to */
	movl	20(%esp),%edx			/* %edx = maxlen */
1:
	/*
	 * It suffices to check that the first byte is in user space, because
	 * we look at a page at a time and the end address is on a page
	 * boundary.
	 */
	cmpl	$VM_MAXUSER_ADDRESS,%edi
	jae	cpystrflt
	movl	%edi,%eax
	shrl	$IDXSHIFT,%eax
	andb	$0xfc,%al
	movb	_PTmap(%eax),%al
	andb	$7,%al
	cmpb	$7,%al
	je	2f

	/* simulate trap */
	pushl	%edx
	pushl	%edi
	call	_trapwrite
	popl	%edi
	popl	%edx
	orl	%eax,%eax
	jnz	cpystrflt

2:	/* copy up to end of this page */
	movl	%edi,%eax
	andl	$NBPG-1,%eax
	movl	$NBPG,%ecx
	subl	%eax,%ecx			/* ecx = NBPG - (src % NBPG) */
	cmpl	%ecx,%edx
	jge	3f
	movl	%edx,%ecx			/* ecx = min(ecx, edx) */
3:
	orl	%ecx,%ecx
	jz	4f
	decl	%ecx
	decl	%edx
	lodsb
	stosb
	orb	%al,%al
	jnz	3b

	/* Success -- 0 byte reached */
	decl	%edx
	xorl	%eax,%eax
	jmp	6f

4:	/* next page */
	orl	%edx,%edx
	jnz	1b

	/* edx is zero -- return ENAMETOOLONG */
	movl	$ENAMETOOLONG,%eax
	jmp	6f

#endif /* USE_486_WRITE_PROTECT */

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
	incl	%edx

1:
	decl	%edx
	jz	4f
	gs
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
	jmp	6f

cpystrflt:
	movl	$EFAULT,%eax
6:
	/* set *lencopied and return %eax */
	movl	_curpcb,%ecx
	movl	$0,PCB_ONFAULT(%ecx)
	movl	20(%esp),%ecx
	subl	%edx,%ecx
	movl	24(%esp),%edx
	orl	%edx,%edx
	jz	7f
	movl	%ecx,(%edx)
7:
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
	orl	%edx,%edx
	jz	7f
	movl	%ecx,(%edx)
7:
	popl	%edi
	popl	%esi
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


/* tlbflush() */
ENTRY(tlbflush)
	movl	%cr3,%eax
	orl	$I386_CR3PAT,%eax
	movl	%eax,%cr3
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


/* rcr2() */
ENTRY(rcr2)
	movl	%cr2,%eax
	ret


/* rcr3() */
ENTRY(rcr3)
	movl	%cr3,%eax
	ret


/* void load_cr3(caddr_t cr3) */
ENTRY(load_cr3)
	movl	4(%esp),%eax
	orl	$I386_CR3PAT,%eax
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

