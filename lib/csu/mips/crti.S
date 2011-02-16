#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/csu/mips/crti.S,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $");

	.section .init,"ax",%progbits
	.align 4
	.globl	_init
	.type	_init,%function
_init:
#ifdef __ABICALLS__
	.set   noreorder
	.cpload $25
	.set   reorder
	subu	sp, sp, 32
	.cprestore 16
	sw	ra, 28(sp)

#else
	subu	sp, sp, 32
	sw	ra, 28(sp)
#endif

	.section .fini,"ax",%progbits
	.align 4
	.globl	_fini
	.type	_fini,%function
_fini:
#ifdef __ABICALLS__
	.set   noreorder
	.cpload $25
	.set   reorder
	subu	sp, sp, 32
	.cprestore 16
	sw	ra, 28(sp)
#else
	subu    sp, sp, 32
	sw	ra, 28(sp)
#endif
