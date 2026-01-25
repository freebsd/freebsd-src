/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2000 Peter Wemm <peter@FreeBSD.org>
 * Copyright 2003 Alan L. Cox <alc@cs.rice.edu>
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
 *                %rdi %esi     %rdx     %rcx        %r8        %r9
 * pdrfork_thread(fdp, pdflags, rfflags, stack_addr, start_fnc, start_arg);
 *
 * fdp			Pointer for the resulting fd location
 * pdflags		Flags as to pdfork.
 * rfflags:		Flags as to rfork.
 * stack_addr:		Top of stack for thread.
 * start_fnc:		Address of thread function to call in child.
 * start_arg:		Argument to pass to the thread function in child.
 */

ENTRY(pdrfork_thread)
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	movq	%r8, %rbx
	movq	%r9, %r12
	movq	%rcx, %r13

	/*
	 * Prepare and execute the thread creation syscall
	 */
	_SYSCALL(pdrfork)
	jb 	2f

	/*
	 * Check to see if we are in the parent or child
	 */
	cmpl	$0, %edx
	jnz	1f
	popq	%r13
	popq	%r12
	popq	%rbx
	ret

	/*
	 * If we are in the child (new thread), then
	 * set-up the call to the internal subroutine.  If it
	 * returns, then call __exit.
	 */
1:
	movq	%r13, %rsp
	movq	%r12, %rdi
	call	*%rbx
	movl	%eax, %edi

	/*
	 * Exit system call
	 */
	_SYSCALL(exit)

	/*
	 * Branch here if the thread creation fails:
	 */
2:
	popq	%r13
	popq	%r12
	popq	%rbx
	jmp	HIDENAME(cerror)
END(pdrfork_thread)

	.section .note.GNU-stack,"",%progbits
