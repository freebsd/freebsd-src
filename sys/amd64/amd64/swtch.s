/*-
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#include "opt_npx.h"

#include <machine/asmacros.h>

#ifdef SMP
#include <machine/apic.h>
#include <machine/smptests.h>			/* CHEAP_TPR, GRAB_LOPRIO */
#endif /* SMP */

#include "assym.s"

/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

	.data

	.globl	panic

#if defined(SWTCH_OPTIM_STATS)
	.globl	swtch_optim_stats, tlb_flush_count
swtch_optim_stats:	.long	0		/* number of _swtch_optims */
tlb_flush_count:	.long	0
#endif

	.text

/*
 * cpu_throw()
 */
ENTRY(cpu_throw)
	jmp	sw1

/*
 * cpu_switch()
 */
ENTRY(cpu_switch)
	
	/* Switch to new process.  First, save context as needed. */
	movl	PCPU(CURTHREAD),%ecx

	/* If no process to save, don't save it (XXX shouldn't happen). */
	testl	%ecx,%ecx
	jz	sw1

	movl	TD_PROC(%ecx), %eax
	movl	P_VMSPACE(%eax), %edx
	movl	PCPU(CPUID), %eax
	btrl	%eax, VM_PMAP+PM_ACTIVE(%edx)

	movl	TD_PCB(%ecx),%edx

	movl	(%esp),%eax			/* Hardware registers */
	movl	%eax,PCB_EIP(%edx)
	movl	%ebx,PCB_EBX(%edx)
	movl	%esp,PCB_ESP(%edx)
	movl	%ebp,PCB_EBP(%edx)
	movl	%esi,PCB_ESI(%edx)
	movl	%edi,PCB_EDI(%edx)
	movl	%gs,PCB_GS(%edx)

	/* Test if debug registers should be saved. */
	movb    PCB_FLAGS(%edx),%al
	andb    $PCB_DBREGS,%al
	jz      1f                              /* no, skip over */
	movl    %dr7,%eax                       /* yes, do the save */
	movl    %eax,PCB_DR7(%edx)
	andl    $0x0000fc00, %eax               /* disable all watchpoints */
	movl    %eax,%dr7
	movl    %dr6,%eax
	movl    %eax,PCB_DR6(%edx)
	movl    %dr3,%eax
	movl    %eax,PCB_DR3(%edx)
	movl    %dr2,%eax
	movl    %eax,PCB_DR2(%edx)
	movl    %dr1,%eax
	movl    %eax,PCB_DR1(%edx)
	movl    %dr0,%eax
	movl    %eax,PCB_DR0(%edx)
1:
 
#ifdef SMP
	/* XXX FIXME: we should be saving the local APIC TPR */
#endif /* SMP */

#ifdef DEV_NPX
	/* have we used fp, and need a save? */
	cmpl	%ecx,PCPU(NPXTHREAD)
	jne	1f
	addl	$PCB_SAVEFPU,%edx		/* h/w bugs make saving complicated */
	pushl	%edx
	call	npxsave				/* do it in a big C function */
	popl	%eax
1:
#endif	/* DEV_NPX */

	/* Save is done.  Now choose a new thread. */
	/* XXX still trashing space above the old "Top Of Stack". */
sw1:

#ifdef SMP
	/* Stop scheduling if smp_active goes to zero and we are not the BSP. */
	cmpl	$0,smp_active
	jne	1f
	cmpl	$0,PCPU(CPUID)
	je	1f
	/* Idle thread can run on any kernel context. */
	movl	PCPU(IDLETHREAD), %eax
	jmp	sw1b
1:
#endif

	/*
	 * Choose a new thread to schedule.  choosethread() returns idlethread
	 * if it cannot find another thread to run.
	 */
sw1a:
	call	choosethread			/* trash ecx, edx, ret eax */

#ifdef INVARIANTS
	testl	%eax,%eax			/* no thread? */
	jz	badsw3				/* no, panic */
#endif

sw1b:
	movl	%eax,%ecx

#ifdef	INVARIANTS
	movl	TD_PROC(%ecx), %eax		/* XXXKSE */
	cmpb	$SRUN,P_STAT(%eax)
	jne	badsw2
#endif

	movl	TD_PCB(%ecx),%edx

#if defined(SWTCH_OPTIM_STATS)
	incl	swtch_optim_stats
#endif

	/* switch address space */
	movl	%cr3,%ebx
	cmpl	PCB_CR3(%edx),%ebx
	je	4f
#if defined(SWTCH_OPTIM_STATS)
	decl	swtch_optim_stats
	incl	tlb_flush_count
#endif
	movl	PCB_CR3(%edx),%ebx
	movl	%ebx,%cr3			/* Load new page tables. */
4:

	movl	PCPU(CPUID), %esi
	cmpl	$0, PCB_EXT(%edx)		/* has pcb extension? */
	je	1f
	btsl	%esi, private_tss		/* mark use of private tss */
	movl	PCB_EXT(%edx), %edi		/* new tss descriptor */
	jmp	2f
1:
	/* update common_tss.tss_esp0 pointer */
	leal	-16(%edx), %ebx			/* leave space for vm86 */
	movl	%ebx, PCPU(COMMON_TSS) + TSS_ESP0 /* stack is below pcb */

	btrl	%esi, private_tss
	jae	3f
	PCPU_ADDR(COMMON_TSSD, %edi)
2:
	/* Move correct tss descriptor into GDT slot, then reload tr. */
	movl	PCPU(TSS_GDT), %ebx		/* entry in GDT */
	movl	0(%edi), %eax
	movl	%eax, 0(%ebx)
	movl	4(%edi), %eax
	movl	%eax, 4(%ebx)
	movl	$GPROC0_SEL*8, %esi		/* GSEL(entry, SEL_KPL) */
	ltr	%si
3:
	/* Note in a vmspace that this cpu is using it. */
	movl	TD_PROC(%ecx),%eax		/* XXXKSE proc from thread */
	movl	P_VMSPACE(%eax), %ebx
	movl	PCPU(CPUID), %eax
	btsl	%eax, VM_PMAP+PM_ACTIVE(%ebx)

	/* restore context */
	movl	PCB_EBX(%edx),%ebx
	movl	PCB_ESP(%edx),%esp
	movl	PCB_EBP(%edx),%ebp
	movl	PCB_ESI(%edx),%esi
	movl	PCB_EDI(%edx),%edi
	movl	PCB_EIP(%edx),%eax
	movl	%eax,(%esp)

#ifdef SMP
#ifdef GRAB_LOPRIO				/* hold LOPRIO for INTs */
#ifdef CHEAP_TPR
	movl	$0, lapic+LA_TPR
#else
	andl	$~APIC_TPR_PRIO, lapic+LA_TPR
#endif /** CHEAP_TPR */
#endif /** GRAB_LOPRIO */
#endif /* SMP */
	movl	%edx, PCPU(CURPCB)
	movl	%ecx, PCPU(CURTHREAD)		/* into next thread */

#ifdef SMP
	/* XXX FIXME: we should be restoring the local APIC TPR */
#endif /* SMP */

	movl	TD_PROC(%ecx),%eax
	cmpl    $0,P_MD+MD_LDT(%eax)		/* Have an LDT? */
	jnz	1f				/* Yes, use it. */
	movl	_default_ldt,%eax		/* Otherwise, use default. */
	cmpl	PCPU(CURRENTLDT),%eax
	je	2f
	lldt	_default_ldt
	movl	%eax,PCPU(CURRENTLDT)
	jmp	2f

1:	pushl	%edx				/* Preserve pointer to pcb. */
	addl	$P_MD,%eax			/* Pointer to mdproc is arg. */
	pushl	%eax
	call	set_user_ldt			/* Check and load the ldt. */
	addl	$4,%esp
	popl	%edx
2:

	/* This must be done after loading the user LDT. */
	.globl	cpu_switch_load_gs
cpu_switch_load_gs:
	movl	PCB_GS(%edx),%gs

	/* test if debug regisers should be restored */
	movb    PCB_FLAGS(%edx),%al
	andb    $PCB_DBREGS,%al
	jz      1f                              /* no, skip over */
	movl    PCB_DR6(%edx),%eax              /* yes, do the restore */
	movl    %eax,%dr6
	movl    PCB_DR3(%edx),%eax
	movl    %eax,%dr3
	movl    PCB_DR2(%edx),%eax
	movl    %eax,%dr2
	movl    PCB_DR1(%edx),%eax
	movl    %eax,%dr1
	movl    PCB_DR0(%edx),%eax
	movl    %eax,%dr0
	movl	%dr7,%eax                	/* load dr7 so as not to */
	andl    $0x0000fc00,%eax         	/* disturb reserved bits */
	movl    PCB_DR7(%edx),%ecx
	andl	$~0x0000fc00,%ecx	/* re-enable the restored watchpoints */
	orl     %ecx,%eax
	movl    %eax,%dr7
1:
	ret

#ifdef INVARIANTS
badsw2:
	pushl	$sw0_2
	call	panic

sw0_2:	.asciz	"cpu_switch: not SRUN"

badsw3:
	pushl	$sw0_3
	call	panic

sw0_3:	.asciz	"cpu_switch: choosethread returned NULL"
#endif

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/* fetch PCB */
	movl	4(%esp),%ecx

	/* caller's return address - child won't execute this routine */
	movl	(%esp),%eax
	movl	%eax,PCB_EIP(%ecx)

	movl	%cr3,%eax
	movl	%eax,PCB_CR3(%ecx)

	movl	%ebx,PCB_EBX(%ecx)
	movl	%esp,PCB_ESP(%ecx)
	movl	%ebp,PCB_EBP(%ecx)
	movl	%esi,PCB_ESI(%ecx)
	movl	%edi,PCB_EDI(%ecx)
	movl	%gs,PCB_GS(%ecx)

#ifdef DEV_NPX
	/*
	 * If npxthread == NULL, then the npx h/w state is irrelevant and the
	 * state had better already be in the pcb.  This is true for forks
	 * but not for dumps (the old book-keeping with FP flags in the pcb
	 * always lost for dumps because the dump pcb has 0 flags).
	 *
	 * If npxthread != NULL, then we have to save the npx h/w state to
	 * npxthread's pcb and copy it to the requested pcb, or save to the
	 * requested pcb and reload.  Copying is easier because we would
	 * have to handle h/w bugs for reloading.  We used to lose the
	 * parent's npx state for forks by forgetting to reload.
	 */
	pushfl
	cli
	movl	PCPU(NPXTHREAD),%eax
	testl	%eax,%eax
	je	1f

	pushl	%ecx
	movl	TD_PCB(%eax),%eax
	leal	PCB_SAVEFPU(%eax),%eax
	pushl	%eax
	pushl	%eax
	call	npxsave
	addl	$4,%esp
	popl	%eax
	popl	%ecx

	pushl	$PCB_SAVEFPU_SIZE
	leal	PCB_SAVEFPU(%ecx),%ecx
	pushl	%ecx
	pushl	%eax
	call	bcopy
	addl	$12,%esp
1:
	popfl
#endif	/* DEV_NPX */

	ret
