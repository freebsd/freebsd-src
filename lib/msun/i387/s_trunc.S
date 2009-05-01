/*
 * Based on code written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/msun/i387/s_trunc.S,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $")

ENTRY(trunc)
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp

	fstcw	-4(%ebp)		/* store fpu control word */
	movw	-4(%ebp),%dx
	orw	$0x0c00,%dx		/* round towards -oo */
	movw	%dx,-8(%ebp)
	fldcw	-8(%ebp)		/* load modfied control word */

	fldl	8(%ebp)			/* round */
	frndint

	fldcw	-4(%ebp)		/* restore original control word */

	leave
	ret
