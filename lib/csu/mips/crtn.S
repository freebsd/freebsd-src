#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/csu/mips/crtn.S,v 1.1.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $");
	.section .init,"ax",%progbits
	lw	ra, 28(sp)
	.set	noreorder
	j	ra
	addu	sp, sp, 32
	.set	reorder

	.section .fini,"ax",%progbits
	lw	ra, 28(sp)
	.set	noreorder
	j	ra
	addu	sp, sp, 32
	.set	reorder
