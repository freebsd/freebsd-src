#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/csu/arm/crtn.S,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $");
	.section .init,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr

	.section .fini,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr
