/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD: src/lib/msun/i387/e_remainderf.S,v 1.1.2.1 2005/01/15 22:56:55 das Exp $");
/* RCSID("$NetBSD: e_remainderf.S,v 1.2 1995/05/08 23:49:47 jtc Exp $") */

ENTRY(__ieee754_remainderf)
	flds	8(%esp)
	flds	4(%esp)
1:	fprem1
	fstsw	%ax
	sahf
	jp	1b
	fstp	%st(1)
	ret
