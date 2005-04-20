/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD$");
/* RCSID("$NetBSD: e_scalbf.S,v 1.1 1996/07/03 16:50:24 jtc Exp $") */

ENTRY(__ieee754_scalbf)
	flds	8(%esp)
	flds	4(%esp)
	fscale
	ret
