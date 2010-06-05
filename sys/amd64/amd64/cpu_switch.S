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
	movb	$1,PCB_FULL_IRET(%r8)

	movq	(%rsp),%rax			/* Hardware registers */
	movq	%r15,PCB_R15(%r8)
	movq	%r14,PCB_R14(%r8)
	movq	%r13,PCB_R13(%r8)
	movq	%r12,PCB_R12(%r8)
	movq	%rbp,PCB_RBP(%r8)
	movq	%rsp,PCB_RSP(%r8)
	movq	%rbx,PCB_RBX(%r8)
	movq	%rax,PCB_RIP(%r8)

	testl	$PCB_DBREGS,PCB_FLAGS(%r8)
	jnz	store_dr			/* static predict not taken */
done_store_dr:

	/* have we used fp, and need a save? */
	cmpq	%rdi,PCPU(FPCURTHREAD)
	jne	1f
	movq	PCB_SAVEFPU(%r8),%r8
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

	/*
	 * Load ldt register
	 */
	movq	TD_PROC(%rsi),%rcx
	cmpq	$0, P_MD+MD_LDT(%rcx)
	jne	do_ldt
	xorl	%eax,%eax
ld_ldt:	lldt	%ax

	/* Restore fs base in GDT */
	movl	PCB_FSBASE(%r8),%eax
	movq	PCPU(FS32P),%rdx
	movw	%ax,2(%rdx)
	shrl	$16,%eax
	movb	%al,4(%rdx)
	shrl	$8,%eax
	movb	%al,7(%rdx)

	/* Restore gs base in GDT */
	movl	PCB_GSBASE(%r8),%eax
	movq	PCPU(GS32P),%rdx
	movw	%ax,2(%rdx)
	shrl	$16,%eax
	movb	%al,4(%rdx)
	shrl	$8,%eax
	movb	%al,7(%rdx)

do_kthread:
	/* Do we need to reload tss ? */
	movq	PCPU(TSSP),%rax
	movq	PCB_TSSP(%r8),%rdx
	testq	%rdx,%rdx
	cmovzq	PCPU(COMMONTSSP),%rdx
	cmpq	%rax,%rdx
	jne	do_tss
done_tss:
	movq	%r8,PCPU(RSP0)
	movq	%r8,PCPU(CURPCB)
	/* Update the TSS_RSP0 pointer for the next interrupt */
	movq	%r8,COMMON_TSS_RSP0(%rdx)
	movq	%rsi,PCPU(CURTHREAD)		/* into next thread */

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

do_tss:	movq	%rdx,PCPU(TSSP)
	movq	%rdx,%rcx
	movq	PCPU(TSS),%rax
	movw	%rcx,2(%rax)
	shrq	$16,%rcx
	movb	%cl,4(%rax)
	shrq	$8,%rcx
	movb	%cl,7(%rax)
	shrq	$8,%rcx
	movl	%ecx,8(%rax)
	movb	$0x89,5(%rax)	/* unset busy */
	movl	$TSSSEL,%eax
	ltr	%ax
	jmp	done_tss

do_ldt:	movq	PCPU(LDT),%rax
	movq	P_MD+MD_LDT_SD(%rcx),%rdx
	movq	%rdx,(%rax)
	movq	P_MD+MD_LDT_SD+8(%rcx),%rdx
	movq	%rdx,8(%rax)
	movl	$LDTSEL,%eax
	jmp	ld_ldt
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
	movq	PCB_SAVEFPU(%rdi),%rdi
	clts
	fxsave	(%rdi)
	smsw	%ax
	orb	$CR0_TS,%al
	lmsw	%ax

	movq	$PCB_SAVEFPU_SIZE,%rdx	/* arg 3 */
	movq	PCB_SAVEFPU(%rcx),%rsi	/* arg 2 */
	/* arg 1 (%rdi) already loaded */
	call	bcopy
1:
	popfq

	ret
END(savectx)

/*
 * savectx2(xpcb)
 * Update xpcb, saving current processor state.
 */
ENTRY(savectx2)
	/* Fetch XPCB. */
	movq	%rdi,%r8

	/* Save caller's return address. */
	movq	(%rsp),%rax
	movq	%rax,PCB_RIP(%r8)

	movq	%rbx,PCB_RBX(%r8)
	movq	%rsp,PCB_RSP(%r8)
	movq	%rbp,PCB_RBP(%r8)
	movq	%r12,PCB_R12(%r8)
	movq	%r13,PCB_R13(%r8)
	movq	%r14,PCB_R14(%r8)
	movq	%r15,PCB_R15(%r8)

	movq	%cr0,%rax
	movq	%rax,XPCB_CR0(%r8)
	movq	%cr2,%rax
	movq	%rax,XPCB_CR2(%r8)
	movq	%cr4,%rax
	movq	%rax,XPCB_CR4(%r8)

	movq	%dr0,%rax
	movq	%rax,PCB_DR0(%r8)
	movq	%dr1,%rax
	movq	%rax,PCB_DR1(%r8)
	movq	%dr2,%rax
	movq	%rax,PCB_DR2(%r8)
	movq	%dr3,%rax
	movq	%rax,PCB_DR3(%r8)
	movq	%dr6,%rax
	movq	%rax,PCB_DR6(%r8)
	movq	%dr7,%rax
	movq	%rax,PCB_DR7(%r8)

	sgdt	XPCB_GDT(%r8)
	sidt	XPCB_IDT(%r8)
	sldt	XPCB_LDT(%r8)
	str	XPCB_TR(%r8)

	movl	$MSR_FSBASE,%ecx
	rdmsr
	shlq	$32,%rdx
	leaq	(%rax,%rdx),%rax
	movq	%rax,PCB_FSBASE(%r8)
	movl	$MSR_GSBASE,%ecx
	rdmsr
	shlq	$32,%rdx
	leaq	(%rax,%rdx),%rax
	movq	%rax,PCB_GSBASE(%r8)
	movl	$MSR_KGSBASE,%ecx
	rdmsr
	shlq	$32,%rdx
	leaq	(%rax,%rdx),%rax
	movq	%rax,XPCB_KGSBASE(%r8)

	movl	$1, %eax
	ret
END(savectx2)
