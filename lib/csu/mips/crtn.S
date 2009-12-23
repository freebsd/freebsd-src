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
#if defined(__mips_n64) || defined(__mips_n32)
	lw	gp, 16(sp)
#endif
	.set	noreorder
	j	ra
	addu	sp, sp, 32
	.set	reorder
