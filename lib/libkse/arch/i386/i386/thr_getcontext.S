/*-
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
#define	MC_LEN_OFFSET		80	/* offset to mc_len from mcontext */
#define	MC_LEN			640	/* mc_len <machine/ucontext.h> */
#define	MC_FPFMT_OFFSET		84
#define MC_FPFMT_NODEV		0x10000
#define MC_FPFMT_387		0x10001
#define	MC_FPFMT_XMM		0x10002
#define	MC_OWNEDFP_OFFSET	88
#define	MC_OWNEDFP_NONE		0x20000
#define	MC_OWNEDFP_FPU		0x20001
#define	MC_OWNEDFP_PCB		0x20002
#define	MC_FPREGS_OFFSET	96	/* offset to FP regs from mcontext */
#define	MC_FP_CW_OFFSET		96	/* offset to FP control word */

/*
 * int thr_setcontext(mcontext_t *mcp, intptr_t val, intptr_t *loc)
 *
 * Restores the context in mcp.
 *
 * Returns 0 if there are no errors; -1 otherwise
 */
	.weak CNAME(_thr_setcontext)
	.set CNAME(_thr_setcontext),CNAME(__thr_setcontext)
ENTRY(__thr_setcontext)
	movl	4(%esp), %edx		/* get address of mcontext */
	cmpl	$0, %edx		/* check for null pointer */
	jne	1f
	movl	$-1, %eax
	jmp	8f
1:	cmpl	$MC_LEN, MC_LEN_OFFSET(%edx)	/* is context valid? */
	je	2f
	movl	$-1, %eax		/* bzzzt, invalid context */
	jmp	8f
2:	/*movl	4(%edx), %gs*/		/* we don't touch %gs */
	movl	8(%edx), %fs
	movl	12(%edx), %es
	movl	16(%edx), %ds
	movl	76(%edx), %ss
	movl	20(%edx), %edi
	movl	24(%edx), %esi
	movl	28(%edx), %ebp
	movl	%esp, %ecx		/* save current stack in ecx */
	movl	72(%edx), %esp		/* switch to context defined stack */
	pushl	60(%edx)		/* push return address on stack */
	pushl	44(%edx)		/* push ecx on stack */
	push	48(%edx)		/* push eax on stack */
	/*
	 * if (mc_fpowned == MC_OWNEDFP_FPU || mc_fpowned == MC_OWNEDFP_PCB) {
	 *    if (mc_fpformat == MC_FPFMT_387)
	 *        restore 387 FP register format
	 *    else if (mc_fpformat == MC_FPFMT_XMM)
	 *        restore XMM/SSE FP register format
	 * }
	 */
	cmpl	$MC_OWNEDFP_FPU, MC_OWNEDFP_OFFSET(%edx)
	je	3f
	cmpl	$MC_OWNEDFP_PCB, MC_OWNEDFP_OFFSET(%edx)
	jne	5f
3:	cmpl	$MC_FPFMT_387, MC_FPFMT_OFFSET(%edx)
	jne	5f
	frstor	MC_FPREGS_OFFSET(%edx)	/* restore 387 FP regs */
	jmp	5f
4:	cmpl	$MC_FPFMT_XMM, MC_FPFMT_OFFSET(%edx)
	jne	5f
	fxrstor	MC_FPREGS_OFFSET(%edx)	/* restore XMM FP regs */
	jmp	6f
5:	fninit
	fldcw	MC_FP_CW_OFFSET(%edx)
6:	pushl	68(%edx)		/* push flags register on stack*/
	movl	36(%edx), %ebx		/* restore ebx and edx */
	movl	40(%edx), %edx
	movl	12(%ecx), %eax		/* get 3rd arg (loc) */
	cmpl	$0, %eax		/* do nothing if loc == null */
	je	7f
	movl	8(%ecx), %ecx		/* get 2nd arg (val) */
	movl	%ecx, (%eax)		/* set loc = val */
7:	popfl				/* restore flags after test */
	popl	%eax			/* restore eax and ecx last */
	popl	%ecx
8:	ret

/*
 * int thr_getcontext(mcontext_t *mcp);
 *
 * Returns -1 if there is an error, 0 no errors; 1 upon return
 * from a setcontext().
 */
	.weak CNAME(_thr_getcontext)
	.set CNAME(_thr_getcontext),CNAME(__thr_getcontext)
ENTRY(__thr_getcontext)
	pushl	%edx			/* save edx */
	movl	8(%esp), %edx		/* get address of mcontext */
	cmpl	$0, %edx		/* check for null pointer */
	jne	1f
	popl	%edx			/* restore edx and stack */
	movl	$-1, %eax
	jmp	2f
1:	/*movl	%gs, 4(%edx)*/		/* we don't touch %gs */
	movl	%fs, 8(%edx)
	movl	%es, 12(%edx)
	movl	%ds, 16(%edx)
	movl	%ss, 76(%edx)
	movl	%edi, 20(%edx)
	movl	%esi, 24(%edx)
	movl	%ebp, 28(%edx)
	movl	%ebx, 36(%edx)
	movl	$1, 48(%edx)		/* store successful return in eax */
	popl	%eax			/* get saved value of edx */
	movl	%eax, 40(%edx)		/* save edx */
	movl	%ecx, 44(%edx)
	movl	(%esp), %eax		/* get return address */
	movl	%eax, 60(%edx)		/* save return address */
	fnstcw	MC_FP_CW_OFFSET(%edx)
	movl	$MC_LEN, MC_LEN_OFFSET(%edx)
	movl	$MC_FPFMT_NODEV, MC_FPFMT_OFFSET(%edx)	/* no FP */
	movl	$MC_OWNEDFP_NONE, MC_OWNEDFP_OFFSET(%edx) /* no FP */
	pushfl
	popl	%eax			/* get eflags */
	movl	%eax, 68(%edx)		/* store eflags */
	movl	%esp, %eax		/* setcontext pushes the return  */
	addl	$4, %eax		/*   address onto the top of the */
	movl	%eax, 72(%edx)		/*   stack; account for this     */
	movl	40(%edx), %edx		/* restore edx -- is this needed? */
	xorl	%eax, %eax		/* return 0 */
2:	ret
