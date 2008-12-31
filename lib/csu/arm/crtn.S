#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/csu/arm/crtn.S,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $");
	.section .init,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr

	.section .fini,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr
