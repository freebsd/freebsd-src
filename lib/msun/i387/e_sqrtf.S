/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD: src/lib/msun/i387/e_sqrtf.S,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $");
/* RCSID("$NetBSD: e_sqrtf.S,v 1.2 1995/05/08 23:50:14 jtc Exp $") */

ENTRY(sqrtf)
	flds	4(%esp)
	fsqrt
	ret
