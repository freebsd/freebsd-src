/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2000 Peter Wemm <peter@FreeBSD.org>
 * Copyright 2026 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by
 * Konstantin Belousov <kib@FreeBSD.org> under sponsorship from
 * the FreeBSD Foundation.
 */

#include <machine/asm.h>
/*
 * With thanks to John Dyson for the original version of this.
 */

#include <SYS.h>

/*
 *              8    12      16       20          24         28
 * rfork_thread(fdp, pdflags rfflags, stack_addr, start_fnc, start_arg);
 *
 * fdp			Pointer for the resulting fd location
 * pdflags		Flags as to pdfork.
 * rfflags:		Flags as to rfork.
 * stack_addr:		Top of stack for thread.
 * start_fnc:		Address of thread function to call in child.
 * start_arg:		Argument to pass to the thread function in child.
 */

ENTRY(pdrfork_thread)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%esi

	/*
	 * Push thread info onto the new thread's stack
	 */
	movl	20(%ebp), %esi	# get stack addr

	subl	$4, %esi
	movl	28(%ebp), %eax	# get start argument
	movl	%eax, (%esi)

	subl	$4, %esi
	movl	24(%ebp), %eax	# get start thread address
	movl	%eax, (%esi)

	/*
	 * Prepare and execute the thread creation syscall
	 */
	pushl	16(%ebp)
	pushl	12(%ebp)
	pushl	8(%ebp)
	pushl	$0
	_SYSCALL(pdrfork)
	jb 	2f

	/*
	 * Check to see if we are in the parent or child
	 */
	cmpl	$0, %edx
	jnz	1f
	addl	$16, %esp
	popl	%esi
	movl	%ebp, %esp
	popl	%ebp
	ret
	.p2align 2

	/*
	 * If we are in the child (new thread), then
	 * set-up the call to the internal subroutine.  If it
	 * returns, then call __exit.
	 */
1:
	movl	%esi,%esp
	popl	%eax
	call	*%eax
	addl	$4, %esp

	/*
	 * Exit system call
	 */
	pushl	%eax
	pushl	$0
	_SYSCALL(exit)

	/*
	 * Branch here if the thread creation fails:
	 */
2:
	addl	$16, %esp
	popl	%esi
	movl	%ebp, %esp
	popl	%ebp
	jmp	HIDENAME(cerror)
END(pdrfork_thread)

	.section .note.GNU-stack,"",%progbits
