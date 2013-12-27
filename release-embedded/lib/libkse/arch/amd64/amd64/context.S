/*
 * Copyright (c) 2003 Daniel Eischen <deischen@freebsd.org>.
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
 * The following notes ("cheat sheet") was provided by Peter Wemm.
 *
 *   scratch:
 *   rax     (1st return)
 *   rcx     (4th arg)
 *   rdx     (3rd arg, 2nd return)
 *   rsi     (2nd arg)
 *   rdi     (1st arg)
 *   r8      (5th arg)
 *   r9      (6th arg)
 *   r10     (temp, static chain?)
 *   r11     (temp)
 * 
 *   preserved:
 *   rbx     (base pointer)
 *   rsp     (stack)
 *   rbp     (frame)
 *   r12-r15 (general)
 * 
 *   calls:
 *   rdi     1
 *   rsi     2
 *   rdx     3
 *   rcx     4
 *   r8      5
 *   r9      6
 * 
 *   return:
 *   rax     1
 *   rdx     2
 * 
 *   This means:
 *   arg1 goes in %rdi, arg2 in %rsi, etc. return value is %rax (and
 *   secondary return, eg: pipe(2), in %rdx) %rcx,%rsi,%rdi etc are
 *   trashed by making a call to something. %rbx,%rbp,%r12-15 are the
 *   only registers preserved across a call. Note that unlike i386,
 *   %rsi and %rdi are scratch rather than preserved. FPU is
 *   different, args are in SSE registers rather than the x87 stack.
 * 
 *   Aside from the register calling conventions, amd64 can be treated
 *   very much like i386.  Things like setjmp/longjmp etc were literal
 *   translations from i386 but with the register names updated, etc. 
 *   The main gotcha is that FPU save/restore is in SSE format, which
 *   means a sparse 512 byte FPU context.
 */


/*
 * Where do we define these?
 */
#define	MC_SIZE			800	/* sizeof mcontext_t */
#define	MC_LEN_OFFSET		(25*8)	/* offset to mc_len from mcontext */
#define	MC_FPFMT_OFFSET		(26*8)	/* offset to mc_fpformat from mcontext */
#define	MC_FPFMT_NODEV		0x10000
#define	MC_OWNEDFP_OFFSET	(27*8)	/* offset to mc_ownedfp from mcontext */
#define	MC_OWNEDFP_NONE 	0x20000
#define	MC_OWNEDFP_FPU		0x20001
#define	MC_OWNEDFP_PCB		0x20002
#define	MC_FPREGS_OFFSET	(28*8)	/* offset to FP registers */
#define	MC_FP_CW_OFFSET		(28*8)	/* offset to FP control word */

#define	MC_RDI		(1 * 8)
#define	MC_RSI		(2 * 8)
#define	MC_RDX		(3 * 8)
#define	MC_RCX		(4 * 8)
#define	MC_R8		(5 * 8)
#define	MC_R9		(6 * 8)
#define	MC_RAX		(7 * 8)
#define	MC_RBX		(8 * 8)
#define	MC_RBP		(9 * 8)
#define	MC_R10		(10 * 8)
#define	MC_R11		(11 * 8)
#define	MC_R12		(12 * 8)
#define	MC_R13		(13 * 8)
#define	MC_R14		(14 * 8)
#define	MC_R15		(15 * 8)
#define	MC_FLAGS	(18 * 8)
#define	MC_RIP		(20 * 8)
#define	MC_CS		(21 * 8)
#define	MC_RFLAGS	(22 * 8)
#define	MC_RSP		(23 * 8)
#define	MC_SS		(24 * 8)

#define	REDZONE		128		/* size of the red zone */

/*
 * _amd64_ctx_save(mcontext_t *mcp)
 *
 * No values are saved to mc_trapno, mc_addr, mc_err and mc_cs.
 * For the FPU state, only the floating point control word is stored.
 */
ENTRY(_amd64_save_context)
	cmpq	$0, %rdi		/* check for null pointer */
	jne	1f
	movq	$-1, %rax
	jmp	2f
1:	movq	%rdi, MC_RDI(%rdi)
	movq	%rsi, MC_RSI(%rdi)
	movq	%rdx, MC_RDX(%rdi)
	movq	%rcx, MC_RCX(%rdi)
	movq	%r8, MC_R8(%rdi)
	movq	%r9, MC_R9(%rdi)
	movq	$1, MC_RAX(%rdi)	/* return 1 when restored */
	movq	%rbx, MC_RBX(%rdi)
	movq	%rbp, MC_RBP(%rdi)
	movq	%r10, MC_R10(%rdi)
	movq	%r11, MC_R11(%rdi)
	movq	%r12, MC_R12(%rdi)
	movq	%r13, MC_R13(%rdi)
	movq	%r14, MC_R14(%rdi)
	movq	%r15, MC_R15(%rdi)
	movq	(%rsp), %rax		/* get return address */
	movq	%rax, MC_RIP(%rdi)	/* save return address (%rip) */
	pushfq				/* get flags */
	popq	%rax
	movq	%rax, MC_RFLAGS(%rdi)	/* save flags */
	movq	%rsp, %rax		/* setcontext pushes the return */
	addq	$8, %rax		/*   address onto the stack; */
	movq	%rax, MC_RSP(%rdi)	/*   account for this -- ???. */
	movw	%ss, MC_SS(%rdi)
	fnstcw	MC_FP_CW_OFFSET(%rdi)	/* save FPU control word */
	movq	$MC_OWNEDFP_NONE, MC_OWNEDFP_OFFSET(%rdi) /* no FP */
	movq	$MC_FPFMT_NODEV, MC_FPFMT_OFFSET(%rdi)
	movq	$MC_SIZE, MC_LEN_OFFSET(%rdi)
	xorq	%rax, %rax		/* return 0 */
2:	ret

/*
 * _amd64_ctx_restore(mcontext_t *mcp, intptr_t val, intptr_t *loc);
 */
ENTRY(_amd64_restore_context)
	cmpq	$0, %rdi		/* check for null pointer */
	jne	1f
	movq	$-1, %rax
	jmp	2f
1:	cmpq	$MC_SIZE, MC_LEN_OFFSET(%rdi)	/* is context valid? */
	je	2f
	movq	$-1, %rax		/* bzzzt, invalid context */
	ret
2:	movq	MC_RCX(%rdi), %rcx
	movq	MC_R8(%rdi), %r8
	movq	MC_R9(%rdi), %r9
	movq	MC_RBX(%rdi), %rbx
	movq	MC_RBP(%rdi), %rbp
	movq	MC_R10(%rdi), %r10
	movq	MC_R11(%rdi), %r11
	movq	MC_R12(%rdi), %r12
	movq	MC_R13(%rdi), %r13
	movq	MC_R14(%rdi), %r14
	movq	MC_R15(%rdi), %r15
	/*
	 * if (mc_fpowned == MC_OWNEDFP_FPU || mc_fpowned == MC_OWNEDFP_PCB)
	 *    restore XMM/SSE FP register format
	 */
	cmpq	$MC_OWNEDFP_NONE, MC_OWNEDFP_OFFSET(%rdi)
	je	4f
	cmpq	$MC_OWNEDFP_PCB, MC_OWNEDFP_OFFSET(%rdi)
	je	3f
	cmpq	$MC_OWNEDFP_FPU, MC_OWNEDFP_OFFSET(%rdi)
	jne	4f
3:	fxrstor	MC_FPREGS_OFFSET(%rdi)	/* restore XMM FP regs */
	jmp	5f
4:	fninit
	fldcw	MC_FP_CW_OFFSET(%rdi)
5:	movq	MC_RSP(%rdi), %rsp	/* switch to context stack */
	subq	$REDZONE, %rsp
	movq	MC_RIP(%rdi), %rax	/* return address on stack */
	pushq	%rax
	movq	MC_RDI(%rdi), %rax	/* rdi on stack */
	pushq	%rax
	movq	MC_RDX(%rdi), %rax	/* rdx on stack */
	pushq	%rax
	movq	MC_RSI(%rdi), %rax	/* rsi on stack */
	pushq	%rax
	movq	MC_RFLAGS(%rdi), %rax	/* flags on stack*/
	pushq	%rax
	movq	MC_RAX(%rdi), %rax	/* restore rax */
	/* At this point we're done with the context. */
	cmpq	$0, %rdx		/* set *loc to val */
	je	6f
	movq	%rsi, (%rdx)
6:	popfq				/* restore flags */
	popq	%rsi			/* restore rsi, rdx, and rdi */
	popq	%rdx
	popq	%rdi
	ret	$REDZONE

