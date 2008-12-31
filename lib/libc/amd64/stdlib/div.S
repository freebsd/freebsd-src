/*	$NetBSD: div.S,v 1.1 2001/06/19 00:25:04 fvdl Exp $	*/

/*-
 * Written by Frank van der Linden (fvdl@wasabisystems.com)
 * Public domain.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/libc/amd64/stdlib/div.S,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

ENTRY(div)
	movl	%edi,%eax
	cltd
	idivl	%esi
	salq	$32,%rdx
	orq	%rdx,%rax
	ret
END(div)
