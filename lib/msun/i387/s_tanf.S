/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD$");
/* RCSID("$NetBSD: s_tanf.S,v 1.3 1995/05/09 00:31:09 jtc Exp $") */

/* A float's domain isn't large enough to require argument reduction. */
ENTRY(tanf)
	flds	4(%esp)
	fptan
	fstp	%st(0)
	ret
