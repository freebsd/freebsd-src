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
#include "opt_sched.h"

/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

	.text

#ifdef SMP
#define LK	lock ;
#else
#define LK
#endif

#if defined(SCHED_ULE) && defined(SMP)
#define	SETLK	xchgq
#else
#define	SETLK	movq
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
	testq	%rdi,%rdi
	jnz	1f
	movq	PCPU(IDLETHREAD),%rdi
1:
	movq	TD_PCB(%rdi),%r8		/* Old pcb */
	movl	PCPU(CPUID), %eax
	movq	PCB_FSBASE(%r8),%r9
	movq	PCB_GSBASE(%r8),%r10
	/* release bit from old pm_active */
	movq	TD_PROC(%rdi), %rdx		/* oldtd->td_proc */
	movq	P_VMSPACE(%rdx), %rdx		/* proc->p_vmspace */
	LK btrl	%eax, VM_PMAP+PM_ACTIVE(%rdx)	/* clear old */
	movq	TD_PCB(%rsi),%r8		/* newtd->td_proc */
	movq	PCB_CR3(%r8),%rdx
	movq	%rdx,%cr3			/* new address space */
	jmp	swact
END(cpu_throw)

/*
 * cpu_switch(old, new, mtx)
 *
 * Save the current thread state, then select the next thread to run
 * and load its state.
 * %rdi = oldtd
 * %rsi = newtd
 * %rdx = mtx
 */
ENTRY(cpu_switch)
	/* Switch to new thread.  First, save context. */
	movq	TD_PCB(%rdi),%r8

	movq	(%rsp),%rax			/* Hardware registers */
	movq	%r15,PCB_R15(%r8)
	movq	%r14,PCB_R14(%r8)
	movq	%r13,PCB_R13(%r8)
	movq	%r12,PCB_R12(%r8)
	movq	%rbp,PCB_RBP(%r8)
	movq	%rsp,PCB_RSP(%r8)
	movq	%rbx,PCB_RBX(%r8)
	movq	%rax,PCB_RIP(%r8)

	/*
	 * Reread fs and gs bases. Explicit fs segment register load
	 * by the usermode code may change actual fs base without
	 * updating pcb_{fs,gs}base.
	 *
	 * %rdx still contains the mtx, save %rdx around rdmsr.
	 */
	movq	%rdx,%r11
	movl	$MSR_FSBASE,%ecx
	rdmsr
	shlq	$32,%rdx
	leaq	(%rax,%rdx),%r9
	movl	$MSR_KGSBASE,%ecx
	rdmsr
	shlq	$32,%rdx
	leaq	(%rax,%rdx),%r10
	movq	%r11,%rdx

	testl	$PCB_32BIT,PCB_FLAGS(%r8)
	jnz	store_seg
done_store_seg:

	testl	$PCB_DBREGS,PCB_FLAGS(%r8)
	jnz	store_dr			/* static predict not taken */
done_store_dr:

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
	movq	PCB_CR3(%r8),%rcx
	movq	%cr3,%rax
	cmpq	%rcx,%rax			/* Same address space? */
	jne	swinact
	SETLK	%rdx, TD_LOCK(%rdi)		/* Release the old thread */
	jmp	sw1
swinact:
	movq	%rcx,%cr3			/* new address space */
	movl	PCPU(CPUID), %eax
	/* Release bit from old pmap->pm_active */
	movq	TD_PROC(%rdi), %rcx		/* oldproc */
	movq	P_VMSPACE(%rcx), %rcx
	LK btrl	%eax, VM_PMAP+PM_ACTIVE(%rcx)	/* clear old */
	SETLK	%rdx, TD_LOCK(%rdi)		/* Release the old thread */
swact:
	/* Set bit in new pmap->pm_active */
	movq	TD_PROC(%rsi),%rdx		/* newproc */
	movq	P_VMSPACE(%rdx), %rdx
	LK btsl	%eax, VM_PMAP+PM_ACTIVE(%rdx)	/* set new */

sw1:
#if defined(SCHED_ULE) && defined(SMP)
	/* Wait for the new thread to become unblocked */
	movq	$blocked_lock, %rdx
1:
	movq	TD_LOCK(%rsi),%rcx
	cmpq	%rcx, %rdx
	pause
	je	1b
#endif
	/*
	 * At this point, we've switched address spaces and are ready
	 * to load up the rest of the next context.
	 */

	/* Skip loading user fsbase/gsbase for kthreads */
	testl	$TDP_KTHREAD,TD_PFLAGS(%rsi)
	jnz	do_kthread

	testl	$PCB_32BIT,PCB_FLAGS(%r8)
	jnz	load_seg
done_load_seg:

	cmpq	PCB_FSBASE(%r8),%r9
	jz	1f
	/* Restore userland %fs */
	movl	$MSR_FSBASE,%ecx
	movl	PCB_FSBASE(%r8),%eax
	movl	PCB_FSBASE+4(%r8),%edx
	wrmsr
1:
	cmpq	PCB_GSBASE(%r8),%r10
	jz	2f
	/* Restore userland %gs */
	movl	$MSR_KGSBASE,%ecx
	movl	PCB_GSBASE(%r8),%eax
	movl	PCB_GSBASE+4(%r8),%edx
	wrmsr
2:

do_tss:
	/* Update the TSS_RSP0 pointer for the next interrupt */
	movq	PCPU(TSSP), %rax
	movq	%r8, PCPU(RSP0)
	movq	%r8, PCPU(CURPCB)
	addq	$COMMON_TSS_RSP0, %rax
	movq	%rsi, PCPU(CURTHREAD)		/* into next thread */
	movq	%r8, (%rax)

	/* Test if debug registers should be restored. */
	testl	$PCB_DBREGS,PCB_FLAGS(%r8)
	jnz	load_dr				/* static predict not taken */
done_load_dr:

	/* Restore context. */
	movq	PCB_R15(%r8),%r15
	movq	PCB_R14(%r8),%r14
	movq	PCB_R13(%r8),%r13
	movq	PCB_R12(%r8),%r12
	movq	PCB_RBP(%r8),%rbp
	movq	PCB_RSP(%r8),%rsp
	movq	PCB_RBX(%r8),%rbx
	movq	PCB_RIP(%r8),%rax
	movq	%rax,(%rsp)
	ret

	/*
	 * We order these strangely for several reasons.
	 * 1: I wanted to use static branch prediction hints
	 * 2: Most athlon64/opteron cpus don't have them.  They define
	 *    a forward branch as 'predict not taken'.  Intel cores have
	 *    the 'rep' prefix to invert this.
	 * So, to make it work on both forms of cpu we do the detour.
	 * We use jumps rather than call in order to avoid the stack.
	 */

do_kthread:
	/*
	 * Copy old fs/gsbase to new kthread pcb for future switches
	 * This maintains curpcb->pcb_[fg]sbase as caches of the MSR
	 */
	movq	%r9,PCB_FSBASE(%r8)
	movq	%r10,PCB_GSBASE(%r8)
	jmp	do_tss

store_seg:
	movl	%gs,PCB_GS(%r8)
	testl	$PCB_GS32BIT,PCB_FLAGS(%r8)
	jnz	2f
1:	movl	%ds,PCB_DS(%r8)
	movl	%es,PCB_ES(%r8)
	movl	%fs,PCB_FS(%r8)
	jmp	done_store_seg
2:	movq	PCPU(GS32P),%rax
	movq	(%rax),%rax
	movq	%rax,PCB_GS32SD(%r8)
	jmp	1b

load_seg:
	testl	$PCB_GS32BIT,PCB_FLAGS(%r8)
	jnz	2f
1:	movl	$MSR_GSBASE,%ecx
	rdmsr
	movl	PCB_GS(%r8),%gs
	wrmsr
	movl	PCB_DS(%r8),%ds
	movl	PCB_ES(%r8),%es
	movl	PCB_FS(%r8),%fs
	jmp	done_load_seg
	/* Restore userland %gs while preserving kernel gsbase */
2:	movq	PCPU(GS32P),%rax
	movq	PCB_GS32SD(%r8),%rcx
	movq	%rcx,(%rax)
	jmp	1b

store_dr:
	movq	%dr7,%rax			/* yes, do the save */
	movq	%dr0,%r15
	movq	%dr1,%r14
	movq	%dr2,%r13
	movq	%dr3,%r12
	movq	%dr6,%r11
	andq	$0x0000fc00, %rax		/* disable all watchpoints */
	movq	%r15,PCB_DR0(%r8)
	movq	%r14,PCB_DR1(%r8)
	movq	%r13,PCB_DR2(%r8)
	movq	%r12,PCB_DR3(%r8)
	movq	%r11,PCB_DR6(%r8)
	movq	%rax,PCB_DR7(%r8)
	movq	%rax,%dr7
	jmp	done_store_dr

load_dr:
	movq	%dr7,%rax
	movq	PCB_DR0(%r8),%r15
	movq	PCB_DR1(%r8),%r14
	movq	PCB_DR2(%r8),%r13
	movq	PCB_DR3(%r8),%r12
	movq	PCB_DR6(%r8),%r11
	movq	PCB_DR7(%r8),%rcx
	movq	%r15,%dr0
	movq	%r14,%dr1
	/* Preserve reserved bits in %dr7 */
	andq	$0x0000fc00,%rax
	andq	$~0x0000fc00,%rcx
	movq	%r13,%dr2
	movq	%r12,%dr3
	orq	%rcx,%rax
	movq	%r11,%dr6
	movq	%rax,%dr7
	jmp	done_load_dr

END(cpu_switch)
	
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
END(savectx)
