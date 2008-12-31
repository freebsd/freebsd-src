#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/csu/arm/crti.S,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $");

	.section .init,"ax",%progbits
	.align 4
	.globl	_init
	.type	_init,%function
_init:
	mov	ip, sp
	stmdb	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4

	.section .fini,"ax",%progbits
	.align 4
	.globl	_fini
	.type	_fini,%function
_fini:
	mov	ip, sp
	stmdb	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4

