/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD$");
/* RCSID("$NetBSD: s_sinf.S,v 1.3 1995/05/09 00:27:53 jtc Exp $") */

/* A float's domain isn't large enough to require argument reduction. */
ENTRY(sinf)
	flds	4(%esp)
	fsin
	ret
