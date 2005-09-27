/*-
 * Copyright (c) 2003 Peter Wemm.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <machine/asmacros.h>
#include <machine/specialreg.h>

#include "assym.s"

/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

	.text

#ifdef SMP
#define LK	lock ;
#else
#define LK
#endif

/*
 * cpu_throw()
 *
 * This is the second half of cpu_switch(). It is used when the current
 * thread is either a dummy or slated to die, and we no longer care
 * about its state.  This is only a slight optimization and is probably
 * not worth it anymore.  Note that we need to clear the pm_active bits so
 * we do need the old proc if it still exists.
 * %rdi = oldtd
 * %rsi = newtd
 */
ENTRY(cpu_throw)
	movl	PCPU(CPUID), %eax
	testq	%rdi,%rdi			/* no thread? */
	jz	1f
	/* release bit from old pm_active */
	movq	TD_PROC(%rdi), %rdx		/* oldtd->td_proc */
	movq	P_VMSPACE(%rdx), %rdx		/* proc->p_vmspace */
	LK btrl	%eax, VM_PMAP+PM_ACTIVE(%rdx)	/* clear old */
1:
	movq	TD_PCB(%rsi),%rdx		/* newtd->td_proc */
	movq	PCB_CR3(%rdx),%rdx
	movq	%rdx,%cr3			/* new address space */
	/* set bit in new pm_active */
	movq	TD_PROC(%rsi),%rdx
	movq	P_VMSPACE(%rdx), %rdx
	LK btsl	%eax, VM_PMAP+PM_ACTIVE(%rdx)	/* set new */
	jmp	sw1

/*
 * cpu_switch(old, new)
 *
 * Save the current thread state, then select the next thread to run
 * and load its state.
 * %rdi = oldtd
 * %rsi = newtd
 */
ENTRY(cpu_switch)
	/* Switch to new thread.  First, save context. */
	movq	TD_PCB(%rdi),%r8

	movq	(%rsp),%rax			/* Hardware registers */
	movq	%rax,PCB_RIP(%r8)
	movq	%rbx,PCB_RBX(%r8)
	movq	%rsp,PCB_RSP(%r8)
	movq	%rbp,PCB_RBP(%r8)
	movq	%r12,PCB_R12(%r8)
	movq	%r13,PCB_R13(%r8)
	movq	%r14,PCB_R14(%r8)
	movq	%r15,PCB_R15(%r8)
	pushfq					/* PSL */
	popq	PCB_RFLAGS(%r8)

	testl	$PCB_32BIT,PCB_FLAGS(%r8)
	jz	1f				/* no, skip over */

	/* Save segment selector numbers */
	movl	%ds,PCB_DS(%r8)
	movl	%es,PCB_ES(%r8)
	movl	%fs,PCB_FS(%r8)
	movl	%gs,PCB_GS(%r8)
	jmp	2f
1:

	/* Save userland %fs */
	movl	$MSR_FSBASE,%ecx
	rdmsr
	movl	%eax,PCB_FSBASE(%r8)
	movl	%edx,PCB_FSBASE+4(%r8)

	/* Save userland %gs */
	movl	$MSR_KGSBASE,%ecx
	rdmsr
	movl	%eax,PCB_GSBASE(%r8)
	movl	%edx,PCB_GSBASE+4(%r8)
2:

	/* Test if debug registers should be saved. */
	testl	$PCB_DBREGS,PCB_FLAGS(%r8)
	jz	1f				/* no, skip over */
	movq	%dr7,%rax			/* yes, do the save */
	movq	%rax,PCB_DR7(%r8)
	andq	$0x0000fc00, %rax		/* disable all watchpoints */
	movq	%rax,%dr7
	movq	%dr6,%rax
	movq	%rax,PCB_DR6(%r8)
	movq	%dr3,%rax
	movq	%rax,PCB_DR3(%r8)
	movq	%dr2,%rax
	movq	%rax,PCB_DR2(%r8)
	movq	%dr1,%rax
	movq	%rax,PCB_DR1(%r8)
	movq	%dr0,%rax
	movq	%rax,PCB_DR0(%r8)
1:

	/* have we used fp, and need a save? */
	cmpq	%rdi,PCPU(FPCURTHREAD)
	jne	1f
	addq	$PCB_SAVEFPU,%r8
	clts
	fxsave	(%r8)
	smsw	%ax
	orb	$CR0_TS,%al
	lmsw	%ax
	xorl	%eax,%eax
	movq	%rax,PCPU(FPCURTHREAD)
1:

	/* Save is done.  Now fire up new thread. Leave old vmspace. */
	movq	TD_PCB(%rsi),%r8

	/* switch address space */
	movq	PCB_CR3(%r8),%rdx
	movq	%cr3,%rax
	cmpq	%rdx,%rax			/* Same address space? */
	je	sw1
	movq	%rdx,%cr3			/* new address space */

	movl	PCPU(CPUID), %eax
	/* Release bit from old pmap->pm_active */
	movq	TD_PROC(%rdi), %rdx		/* oldproc */
	movq	P_VMSPACE(%rdx), %rdx
	LK btrl	%eax, VM_PMAP+PM_ACTIVE(%rdx)	/* clear old */

	/* Set bit in new pmap->pm_active */
	movq	TD_PROC(%rsi),%rdx		/* newproc */
	movq	P_VMSPACE(%rdx), %rdx
	LK btsl	%eax, VM_PMAP+PM_ACTIVE(%rdx)	/* set new */

sw1:
	/*
	 * At this point, we've switched address spaces and are ready
	 * to load up the rest of the next context.
	 */
	movq	TD_PCB(%rsi),%r8

	testl	$PCB_32BIT,PCB_FLAGS(%r8)
	jz	1f				/* no, skip over */

	/* Restore segment selector numbers */
	movl	PCB_DS(%r8),%ds
	movl	PCB_ES(%r8),%es
	movl	PCB_FS(%r8),%fs

	/* Restore userland %gs while preserving kernel gsbase */
	movl	$MSR_GSBASE,%ecx
	rdmsr
	movl	PCB_GS(%r8),%gs
	wrmsr
	jmp	2f
1:

	/* Restore userland %fs */
	movl	$MSR_FSBASE,%ecx
	movl	PCB_FSBASE(%r8),%eax
	movl	PCB_FSBASE+4(%r8),%edx
	wrmsr

	/* Restore userland %gs */
	movl	$MSR_KGSBASE,%ecx
	movl	PCB_GSBASE(%r8),%eax
	movl	PCB_GSBASE+4(%r8),%edx
	wrmsr
2:

	/* Update the TSS_RSP0 pointer for the next interrupt */
	movq	PCPU(TSSP), %rax
	addq	$COMMON_TSS_RSP0, %rax
	leaq	-16(%r8), %rbx
	movq	%rbx, (%rax)
	movq	%rbx, PCPU(RSP0)

	/* Restore context. */
	movq	PCB_RBX(%r8),%rbx
	movq	PCB_RSP(%r8),%rsp
	movq	PCB_RBP(%r8),%rbp
	movq	PCB_R12(%r8),%r12
	movq	PCB_R13(%r8),%r13
	movq	PCB_R14(%r8),%r14
	movq	PCB_R15(%r8),%r15
	movq	PCB_RIP(%r8),%rax
	movq	%rax,(%rsp)
	pushq	PCB_RFLAGS(%r8)
	popfq

	movq	%r8, PCPU(CURPCB)
	movq	%rsi, PCPU(CURTHREAD)		/* into next thread */

	/* Test if debug registers should be restored. */
	testl	$PCB_DBREGS,PCB_FLAGS(%r8)
	jz	1f
	movq	PCB_DR6(%r8),%rax
	movq	%rax,%dr6
	movq	PCB_DR3(%r8),%rax
	movq	%rax,%dr3
	movq	PCB_DR2(%r8),%rax
	movq	%rax,%dr2
	movq	PCB_DR1(%r8),%rax
	movq	%rax,%dr1
	movq	PCB_DR0(%r8),%rax
	movq	%rax,%dr0
	/* But preserve reserved bits in %dr7 */
	movq	%dr7,%rax
	andq	$0x0000fc00,%rax
	movq	PCB_DR7(%r8),%rcx
	andq	$~0x0000fc00,%rcx
	orq	%rcx,%rax
	movq	%rax,%dr7
1:
	ret

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/* Fetch PCB. */
	movq	%rdi,%rcx

	/* Save caller's return address. */
	movq	(%rsp),%rax
	movq	%rax,PCB_RIP(%rcx)

	movq	%cr3,%rax
	movq	%rax,PCB_CR3(%rcx)

	movq	%rbx,PCB_RBX(%rcx)
	movq	%rsp,PCB_RSP(%rcx)
	movq	%rbp,PCB_RBP(%rcx)
	movq	%r12,PCB_R12(%rcx)
	movq	%r13,PCB_R13(%rcx)
	movq	%r14,PCB_R14(%rcx)
	movq	%r15,PCB_R15(%rcx)
	pushfq
	popq	PCB_RFLAGS(%rcx)

	/*
	 * If fpcurthread == NULL, then the fpu h/w state is irrelevant and the
	 * state had better already be in the pcb.  This is true for forks
	 * but not for dumps (the old book-keeping with FP flags in the pcb
	 * always lost for dumps because the dump pcb has 0 flags).
	 *
	 * If fpcurthread != NULL, then we have to save the fpu h/w state to
	 * fpcurthread's pcb and copy it to the requested pcb, or save to the
	 * requested pcb and reload.  Copying is easier because we would
	 * have to handle h/w bugs for reloading.  We used to lose the
	 * parent's fpu state for forks by forgetting to reload.
	 */
	pushfq
	cli
	movq	PCPU(FPCURTHREAD),%rax
	testq	%rax,%rax
	je	1f

	movq	TD_PCB(%rax),%rdi
	leaq	PCB_SAVEFPU(%rdi),%rdi
	clts
	fxsave	(%rdi)
	smsw	%ax
	orb	$CR0_TS,%al
	lmsw	%ax

	movq	$PCB_SAVEFPU_SIZE,%rdx	/* arg 3 */
	leaq	PCB_SAVEFPU(%rcx),%rsi	/* arg 2 */
	/* arg 1 (%rdi) already loaded */
	call	bcopy
1:
	popfq

	ret
