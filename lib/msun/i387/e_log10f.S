/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

__FBSDID("$FreeBSD: src/lib/msun/i387/e_log10f.S,v 1.3.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $");
/* RCSID("$NetBSD: e_log10f.S,v 1.1 1996/07/03 16:50:22 jtc Exp $") */

ENTRY(log10f)
	fldlg2
	flds	4(%esp)
	fyl2x
	ret
END(log10f)
