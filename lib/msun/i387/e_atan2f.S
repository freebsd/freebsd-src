/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD: src/lib/msun/i387/e_atan2f.S,v 1.1.2.1 2005/01/15 22:56:55 das Exp $");
/* RCSID("$NetBSD: e_atan2f.S,v 1.1 1995/05/08 23:35:10 jtc Exp $") */

ENTRY(__ieee754_atan2f)
	flds	4(%esp)
	flds	8(%esp)
	fpatan
	ret
