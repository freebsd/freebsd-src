/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD: src/lib/msun/i387/s_scalbnl.S,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $");
/* RCSID("$NetBSD: s_scalbnf.S,v 1.4 1999/01/02 05:15:40 kristerw Exp $") */

ENTRY(scalbnl)
	fildl	16(%esp)
	fldt	4(%esp)
	fscale
	fstp	%st(1)
	ret
END(scalbnl)

.globl CNAME(ldexpl)
.set	CNAME(ldexpl),CNAME(scalbnl)
