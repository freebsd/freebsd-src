/*
 * This is the analog to the kexec "purgatory" code
 *
 * The goal here is to call the actual kernel entry point with the arguments it
 * expects when kexec calls into it with no arguments. The value of the kernel
 * entry point and arguments r3-r7 are copied into the trampoline text (which
 * can be executed from any address) at bytes 8-32. kexec begins execution
 * of APs at 0x60 bytes past the entry point, executing in a copy relocated
 * to the absolute address 0x60. Here we implement a loop waiting on the release
 * of a lock by the kernel at 0x40.
 * 
 * $FreeBSD$
 */

#include <machine/asm.h>

        .globl  CNAME(kerneltramp),CNAME(szkerneltramp)
CNAME(kerneltramp):
	mflr %r9
	bl 2f
	.space 24	/* branch address, r3-r7 */

. = kerneltramp + 0x40	/* AP spinlock */
	.long 0

. = kerneltramp + 0x60	/* AP entry point */
	li	%r3,0x40
1:	lwz	%r1,0(%r3)
	cmpwi	%r1,0
	beq	1b

	/* Jump into CPU reset */
	li	%r0,0x100
	icbi	0,%r0
	isync
	sync
	ba	0x100

2:			/* Continuation of kerneltramp */
	mflr	%r8
	mtlr	%r9
	lwz	%r3,0(%r8)
	mtctr	%r3
	lwz	%r3,4(%r8)
	lwz	%r4,8(%r8)
	lwz	%r5,12(%r8)
	lwz	%r6,16(%r8)
	lwz	%r7,20(%r8)
	bctr
	
endkerneltramp:

	.data
CNAME(szkerneltramp):
	.long endkerneltramp - CNAME(kerneltramp)
