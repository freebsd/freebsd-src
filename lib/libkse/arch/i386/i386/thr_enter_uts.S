/*
 * Copyright (c) 2002 Jonathan Mini <mini@freebsd.org>.
 * Copyright (c) 2001 Daniel Eischen <deischen@freebsd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

/*
 * Where do we define these?
 */
#define	MC_SIZE			640	/* sizeof mcontext_t */
#define	UC_MC_OFFSET		16	/* offset to mcontext from ucontext */
#define	MC_LEN_OFFSET		80	/* offset to mc_len from mcontext */
#define	MC_FP_CW_OFFSET		96	/* offset to FP control word */
#define	MC_OWNEDFP_OFFSET	88	/* offset to mc_ownedfp from mcontext */
#define	KM_STACK_SP_OFFSET	36	/* offset to km_stack.ss_sp */
#define	KM_STACK_SIZE_OFFSET	40	/* offset to km_stack.ss_sp */
#define	KM_FUNC_OFFSET		32	/* offset to km_func */

/*
 * int _thread_enter_uts(kse_thr_mailbox *tm, kse_mailbox *km);
 *
 * Returns 0 on success, -1 otherwise.
 */
ENTRY(_thread_enter_uts)
	movl	4(%esp), %eax		/* get address of context */
	cmpl	$0, %eax		/* check for null pointer */
	jne	1f
	movl	$-1, %eax
	jmp	2f
1:	pushl	%edx			/* save value of edx */
	movl	%eax, %edx		/* get address of context */
	addl	$UC_MC_OFFSET, %edx	/* add offset to mcontext */
	movl	%gs, 4(%edx)
	movl	%fs, 8(%edx)
	movl	%es, 12(%edx)
	movl	%ds, 16(%edx)
	movl	%edi, 20(%edx)
	movl	%esi, 24(%edx)
	movl	%ebp, 28(%edx)
	movl	%ebx, 36(%edx)
	movl	$0, 48(%edx)		/* store successful return in eax */
	popl	%eax			/* get saved value of edx */
	movl	%eax, 40(%edx)		/* save edx */
	movl	%ecx, 44(%edx)
	movl	(%esp), %eax		/* get return address */
	movl	%eax, 60(%edx)		/* save return address */
	movl	%ss, 76(%edx)
	/*
	 * Don't save floating point registers here.
	 *
	 * This is an explicit call to get the current context, so
	 * the caller is done with the floating point registers.
	 * Contexts formed by involuntary switches, such as signal delivery,
	 * have floating point registers saved by the kernel.
	 */
	fnstcw	MC_FP_CW_OFFSET(%edx)
	movl	$0, MC_OWNEDFP_OFFSET(%edx) /* no FP */
	lahf				/* get eflags */
	movl	%eax, 68(%edx)		/* store eflags */
	movl	%esp, %eax		/* setcontext pushes the return  */
	addl	$4, %eax		/*   address onto the top of the */
	movl	%eax, 72(%edx)		/*   stack; account for this     */
	movl	$MC_SIZE, MC_LEN_OFFSET(%edx) /* context is now valid */
	movl	8(%esp), %edx		/* get address of mailbox */
	movl	KM_STACK_SP_OFFSET(%edx), %eax	/* get bottom of stack */
	addl	KM_STACK_SIZE_OFFSET(%edx), %eax /* add length */
	movl	%eax, %esp		/* switch to the uts's stack */
	pushl	%edx			/* push the address of the mailbox */
	pushl	KM_FUNC_OFFSET(%edx)	/* .. the uts can return to itself */
	pushl	KM_FUNC_OFFSET(%edx)	/* push the address of the uts func */
2:	ret

