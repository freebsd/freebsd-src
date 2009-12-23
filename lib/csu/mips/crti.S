#include <machine/asm.h>
__FBSDID("$FreeBSD$");

	.section .init,"ax",%progbits
	.align 4
	.globl	_init
	.type	_init,%function
	
	/*
	 * The Cavium toolchain apparently has code that references $fp
	 * instead of $sp for the .cprestore 16 macro.  The move doesn't
	 * hurt on other toolchains, so unconditionally compile it in for
	 * now.
	 */
_init:
#ifdef __ABICALLS__
	.set   noreorder
	.cpload $25
	.set   reorder
	subu	sp, sp, 32
	.cprestore 16
	sw	ra, 28(sp)
	move	s8, sp		/* See note above */
#else
	subu	sp, sp, 32
	sw	ra, 28(sp)
#endif

	.section .fini,"ax",%progbits
	.align 4
	.globl	_fini
	.type	_fini,%function
_fini:
	
#if defined (__mips_n64) || defined (__mips_n32)
#ifdef __ABICALLS__
	subu	sp, sp, 32
	sw	gp, 16(sp)
	sw	ra, 28(sp)
	move	s8, sp
#else
	subu	sp, sp, 32
	sw	ra, 28(sp)
#endif /*__ABICALLS__*/

#else
#ifdef __ABICALLS__
	.set   noreorder
	.cpload $25
	.set   reorder
	subu	sp, sp, 32
	move	s8, sp		/* See note above */
	.cprestore 16		/* xxx missing in Cavium's version -- why? */
	sw	ra, 28(sp)
#else
	subu    sp, sp, 32
	sw	ra, 28(sp)
#endif /* __ABICALLS__ */

#endif /* __mips_n64 || __mips_n32 */
