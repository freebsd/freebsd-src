/*	$NetBSD: ldiv.S,v 1.1 2001/06/19 00:25:04 fvdl Exp $	*/

/*-
 * Written by gcc 3.0.
 * Copy/pasted by Frank van der Linden (fvdl@wasabisystems.com)
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/libc/amd64/stdlib/ldiv.S,v 1.1 2007/04/04 01:19:54 jkim Exp $");

ENTRY(ldiv)
	movq	%rdi,%rax
	cqto
	idivq	%rsi
	ret
