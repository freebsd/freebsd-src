#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#if 0
	RCSID("$NetBSD: bcmp.S,v 1.1 2001/06/19 00:25:04 fvdl Exp $")
#endif

ENTRY(bcmp)
	xorl	%eax,%eax		/* clear return value */
	cld				/* set compare direction forward */

	movq	%rdx,%rcx		/* compare by words */
	shrq	$3,%rcx
	repe
	cmpsq
	jne	L1

	movq	%rdx,%rcx		/* compare remainder by bytes */
	andq	$7,%rcx
	repe
	cmpsb
	je	L2

L1:	incl	%eax
L2:	ret
