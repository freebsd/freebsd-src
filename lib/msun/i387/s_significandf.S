/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD: src/lib/msun/i387/s_significandf.S,v 1.1.12.1.2.1 2010/02/10 00:26:20 kensmith Exp $");
/* RCSID("$NetBSD: s_significandf.S,v 1.3 1995/05/09 00:24:07 jtc Exp $") */

ENTRY(significandf)
	flds	4(%esp)
	fxtract
	fstp	%st(1)
	ret
END(significandf)
