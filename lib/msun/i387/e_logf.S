/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD: src/lib/msun/i387/e_logf.S,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $");
/* RCSID("$NetBSD: e_logf.S,v 1.2 1996/07/06 00:15:45 jtc Exp $") */

ENTRY(logf)
	fldln2
	flds	4(%esp)
	fyl2x
	ret
