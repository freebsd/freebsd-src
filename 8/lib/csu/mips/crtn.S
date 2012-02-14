#include <machine/asm.h>
__FBSDID("$FreeBSD$");
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
