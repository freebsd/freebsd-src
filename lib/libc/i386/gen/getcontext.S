/*
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
#define	UC_MC_VALID		0x0001	/* __UC_MC_VALID <machine/ucontext.h> */
#define	UC_FP_VALID		0x0002	/* __UC_FP_VALID <machine/ucontext.h> */
#define	MC_FP_VALID		UC_MC_VALID | UC_FP_VALID
#define	UC_MC_OFFSET		16	/* offset to mcontext from ucontext */
#define	UC_FLAGS_OFFSET		208	/* offset to flags from ucontext */
#define	MC_FLAGS_OFFSET		192	/* offset to flags from mcontext */
#define	MC_FP_REGS_OFFSET	80	/* offset to FP regs from mcontext */
#define	MC_FP_CW_OFFSET		80	/* offset to FP control word */

/*
 * int setcontext(ucontext_t *ucp);
 *
 *   calls sigprocmask(int how, sigset_t *set, sigset_t *oset);
 *
 * Returns 0 if there are no errors; -1 otherwise
 */
	.weak CNAME(setcontext)
	.set CNAME(setcontext),CNAME(__setcontext)
ENTRY(__setcontext)
	movl	4(%esp), %eax		/* get address of context and sigset */
	cmpl	$0, %eax		/* check for null pointer */
	jne	1f
	movl	$-1, %eax
	jmp	5f
1:	testl	$UC_MC_VALID, UC_FLAGS_OFFSET(%eax) /* is context valid? */
	jnz	2f
	movl	$-1, %eax		/* bzzzt, invalid context */
	jmp	5f
2:	PIC_PROLOGUE
	pushl	$0			/* oset = NULL */
	pushl	%eax			/* set = &ucp->uc_sigmask */
	pushl	$3			/* how = SIG_SETMASK */
	call	PIC_PLT(CNAME(_sigprocmask))
	addl	$12, %esp
	PIC_EPILOGUE
	testl	%eax, %eax		/* check for error */
	jnz	5f
	movl	4(%esp), %edx		/* get address of context */
	addl	$UC_MC_OFFSET, %edx	/* add offset to mcontext */
	movl	4(%edx), %gs
	movl	8(%edx), %fs
	movl	12(%edx), %es
	movl	16(%edx), %ds
	movl	20(%edx), %edi
	movl	24(%edx), %esi
	movl	28(%edx), %ebp
	movl	72(%edx), %esp		/* switch to context defined stack */
	subl	$4, %esp		/* leave space for the return address */
	movl	60(%edx), %eax		/* put return address at top of stack */
	movl	%eax, (%esp)
	testl	$UC_FP_VALID, MC_FLAGS_OFFSET(%edx) /* are FP regs valid? */
	jz	3f
	frstor	MC_FP_REGS_OFFSET(%edx) /* restore FP regs */
	jmp	4f
3:	fninit
	fldcw	MC_FP_CW_OFFSET(%edx)
4:	movl	68(%edx), %eax		/* restore flags register */
	sahf
	movl	48(%edx), %eax		/* restore ax, bx, cx, and dx last */
	movl	36(%edx), %ebx
	movl	44(%edx), %ecx
	movl	40(%edx), %edx
5:	ret

/*
 * int getcontext(ucontext_t *ucp);
 *
 *   calls sigprocmask(int how, sigset_t *set, sigset_t *oset);
 *
 * Returns 0 if there are no errors; -1 otherwise
 */
	.weak CNAME(getcontext)
	.set CNAME(getcontext),CNAME(__getcontext)
ENTRY(__getcontext)
	movl	4(%esp), %eax		/* get address of context */
	cmpl	$0, %eax		/* check for null pointer */
	jne	1f
	movl	$-1, %eax
	jmp	2f
	movl	4(%esp), %eax		/* get address of context and sigset */
1:	PIC_PROLOGUE
	pushl	%eax			/* oset = &ucp->uc_sigmask */
	pushl	$0			/* set = NULL */
	pushl	$3			/* how = SIG_SETMASK */
	call	PIC_PLT(CNAME(_sigprocmask))
	addl	$12, %esp
	PIC_EPILOGUE
	testl	%eax, %eax		/* check for error */
	jnz	2f
	pushl	%edx			/* save value of edx */
	movl	8(%esp), %edx		/* get address of context */
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
	/*
	 * XXX - Do we really need to save floating point registers?
	 *
	 * This is an explicit call to get the current context, so
	 * shouldn't the caller be done with the floating point registers?
	 * Contexts formed by involuntary switches, such as signal delivery,
	 * should have floating point registers saved by the kernel.
	 *
	 * As of this writing, the kernel doesn't yet save the FPU state
	 * on signal delivery, so a setcontext on the interrupted context
	 * may yield incorrect results regardless.
	 */
#if 1
	fnstcw	MC_FP_CW_OFFSET(%edx)
	movl	$UC_MC_VALID, MC_FLAGS_OFFSET(%edx) /* mcontext valid, no FP */
#else
	fnsave	MC_FP_REGS_OFFSET(%edx) /* save FP regs */
	movl	$MC_FP_VALID, MC_FLAGS_OFFSET(%edx) /* mcontext and FP valid */
#endif
	lahf				/* get eflags */
	movl	%eax, 68(%edx)		/* store eflags */
	movl	%esp, %eax		/* setcontext pushes the return  */
	addl	$4, %eax		/*   address onto the top of the */
	movl	%eax, 72(%edx)		/*   stack; account for this     */
	movl	40(%edx), %edx		/* restore edx -- is this needed? */
	xorl	%eax, %eax		/* return 0 */
2:	ret
