#include <machine/asm.h>
#if defined(LIBC_SCCS)
	RCSID("$NetBSD: fabs.S,v 1.4 1997/07/16 14:37:16 christos Exp $")
#endif
__FBSDID("$FreeBSD: src/lib/libc/amd64/gen/fabs.S,v 1.1 2003/04/30 16:21:03 obrien Exp $");

/*
 * Ok, this sucks. Is there really no way to push an xmm register onto
 * the FP stack directly?
 */

ENTRY(fabs)
	movsd	%xmm0, -8(%rsp)
	fldl	-8(%rsp)
	fabs
	fstp	-8(%rsp)
	movsd	-8(%rsp),%xmm0
	ret
