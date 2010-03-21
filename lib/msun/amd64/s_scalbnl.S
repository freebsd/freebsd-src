/*
 * Based on code written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/msun/amd64/s_scalbnl.S,v 1.1.10.1.2.1 2010/02/10 00:26:20 kensmith Exp $")
/* RCSID("$NetBSD: s_scalbnf.S,v 1.4 1999/01/02 05:15:40 kristerw Exp $") */

ENTRY(scalbnl)
	movl	%edi,-4(%rsp)
	fildl	-4(%rsp)
	fldt	8(%rsp)
	fscale
	fstp	%st(1)
	ret
END(scalbnl)

.globl	CNAME(ldexpl)
.set	CNAME(ldexpl),CNAME(scalbnl)
