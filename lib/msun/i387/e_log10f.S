/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD$");
/* RCSID("$NetBSD: e_log10f.S,v 1.1 1996/07/03 16:50:22 jtc Exp $") */

ENTRY(__ieee754_log10f)
	fldlg2
	flds	4(%esp)
	fyl2x
	ret
