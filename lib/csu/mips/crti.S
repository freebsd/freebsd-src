#include <machine/asm.h>
__FBSDID("$FreeBSD$");

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
