#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/csu/arm/crtn.S,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $");
	.section .init,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr

	.section .fini,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr
