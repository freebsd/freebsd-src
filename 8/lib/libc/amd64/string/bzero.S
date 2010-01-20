/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 * Adapted for NetBSD/x86_64 by Frank van der Linden <fvdl@wasabisystems.com>
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#if 0
	RCSID("$NetBSD: bzero.S,v 1.2 2003/07/26 19:24:38 salo Exp $")
#endif

ENTRY(bzero)
	cld				/* set fill direction forward */
	xorq	%rax,%rax		/* set fill data to 0 */

	/*
	 * if the string is too short, it's really not worth the overhead
	 * of aligning to word boundries, etc.  So we jump to a plain
	 * unaligned set.
	 */
	cmpq	$16,%rsi
	jb	L1

	movq	%rdi,%rcx		/* compute misalignment */
	negq	%rcx
	andq	$7,%rcx
	subq	%rcx,%rsi
	rep				/* zero until word aligned */
	stosb

	movq	%rsi,%rcx		/* zero by words */
	shrq	$3,%rcx
	andq	$7,%rsi
	rep
	stosq

L1:	movq	%rsi,%rcx		/* zero remainder by bytes */
	rep
	stosb

	ret
END(bzero)
