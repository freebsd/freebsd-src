#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/csu/mips/crtn.S,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $");
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
