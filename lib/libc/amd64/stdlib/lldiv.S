/*	$NetBSD: ldiv.S,v 1.1 2001/06/19 00:25:04 fvdl Exp $	*/

/*-
 * Written by gcc 3.0.
 * Copy/pasted by Frank van der Linden (fvdl@wasabisystems.com)
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/libc/amd64/stdlib/lldiv.S,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

ENTRY(lldiv)
	movq	%rdi,%rax
	cqto
	idivq	%rsi
	ret
END(lldiv)
