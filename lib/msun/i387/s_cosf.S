/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD$");
/* RCSID("$NetBSD: s_cosf.S,v 1.4 1999/07/02 15:37:34 simonb Exp $") */

/* A float's domain isn't large enough to require argument reduction. */
ENTRY(cosf)
	flds	4(%esp)
	fcos
	ret
