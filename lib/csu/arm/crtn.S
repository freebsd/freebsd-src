#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/csu/arm/crtn.S,v 1.2 2004/09/23 23:00:51 cognet Exp $");
	.section .init,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr

	.section .fini,"ax",%progbits
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr
