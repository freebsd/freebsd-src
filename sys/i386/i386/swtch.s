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

#include "npx.h"
#include "opt_user_ldt.h"

#include <sys/rtprio.h>

#include <machine/asmacros.h>
#include <machine/ipl.h>

#ifdef SMP
#include <machine/pmap.h>
#include <machine/smptests.h>		/** GRAB_LOPRIO */
#include <machine/apic.h>
#include <machine/lock.h>
#endif /* SMP */

#include "assym.s"


/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

	.data

	.globl	_hlt_vector
_hlt_vector:	.long	_cpu_idle	/* pointer to halt routine */

	.globl	_panic

#if defined(SWTCH_OPTIM_STATS)
	.globl	_swtch_optim_stats, _tlb_flush_count
_swtch_optim_stats:	.long	0		/* number of _swtch_optims */
_tlb_flush_count:	.long	0
#endif

	.text

/*
 * When no processes are on the runq, cpu_switch() branches to _idle
 * to wait for something to come ready.
 */
	ALIGN_TEXT
	.type	_idle,@function
_idle:
	xorl	%ebp,%ebp
	movl	%ebp,_switchtime

#ifdef SMP

	/* when called, we have the mplock, intr disabled */
	/* use our idleproc's "context" */
	movl	_IdlePTD, %ecx
	movl	%cr3, %eax
	cmpl	%ecx, %eax
	je		2f
#if defined(SWTCH_OPTIM_STATS)
	decl	_swtch_optim_stats
	incl	_tlb_flush_count
#endif
	movl	%ecx, %cr3
2:
	/* Keep space for nonexisting return addr, or profiling bombs */
	movl	$gd_idlestack_top-4, %ecx	
	addl	%fs:0, %ecx
	movl	%ecx, %esp

	/* update common_tss.tss_esp0 pointer */
	movl	%ecx, _common_tss + TSS_ESP0

	movl	_cpuid, %esi
	btrl	%esi, _private_tss
	jae	1f

	movl	$gd_common_tssd, %edi
	addl	%fs:0, %edi

	/* move correct tss descriptor into GDT slot, then reload tr */
	movl	_tss_gdt, %ebx			/* entry in GDT */
	movl	0(%edi), %eax
	movl	%eax, 0(%ebx)
	movl	4(%edi), %eax
	movl	%eax, 4(%ebx)
	movl	$GPROC0_SEL*8, %esi		/* GSEL(entry, SEL_KPL) */
	ltr	%si
1:

	sti

	/*
	 * XXX callers of cpu_switch() do a bogus splclock().  Locking should
	 * be left to cpu_switch().
	 *
	 * NOTE: spl*() may only be called while we hold the MP lock (which 
	 * we do).
	 */
	call	_spl0

	cli

	/*
	 * _REALLY_ free the lock, no matter how deep the prior nesting.
	 * We will recover the nesting on the way out when we have a new
	 * proc to load.
	 *
	 * XXX: we had damn well better be sure we had it before doing this!
	 */
	movl	$FREE_LOCK, %eax
	movl	%eax, _mp_lock

	/* do NOT have lock, intrs disabled */
	.globl	idle_loop
idle_loop:

	cmpl	$0,_smp_active
	jne	1f
	cmpl	$0,_cpuid
	je	1f
	jmp	2f

1:
	call	_procrunnable
	testl	%eax,%eax
	jnz	3f

	/*
	 * Handle page-zeroing in the idle loop.  Called with interrupts
	 * disabled and the MP lock released.  Inside vm_page_zero_idle
	 * we enable interrupts and grab the mplock as required.
	 */
	cmpl	$0,_do_page_zero_idle
	je	2f

	call	_vm_page_zero_idle		/* internal locking */
	testl	%eax, %eax
	jnz	idle_loop
2:

	/* enable intrs for a halt */
	movl	$0, lapic_tpr			/* 1st candidate for an INT */
	call	*_hlt_vector			/* wait for interrupt */
	cli
	jmp	idle_loop

	/*
	 * Note that interrupts must be enabled while obtaining the MP lock
	 * in order to be able to take IPI's while blocked.
	 */
3:
#ifdef GRAB_LOPRIO
	movl	$LOPRIO_LEVEL, lapic_tpr	/* arbitrate for INTs */
#endif
	sti
	call	_get_mplock
	cli
	call	_procrunnable
	testl	%eax,%eax
	CROSSJUMP(jnz, sw1a, jz)
	call	_rel_mplock
	jmp	idle_loop

#else /* !SMP */

	movl	$HIDENAME(tmpstk),%esp
#if defined(OVERLY_CONSERVATIVE_PTD_MGMT)
#if defined(SWTCH_OPTIM_STATS)
	incl	_swtch_optim_stats
#endif
	movl	_IdlePTD, %ecx
	movl	%cr3, %eax
	cmpl	%ecx, %eax
	je		2f
#if defined(SWTCH_OPTIM_STATS)
	decl	_swtch_optim_stats
	incl	_tlb_flush_count
#endif
	movl	%ecx, %cr3
2:
#endif

	/* update common_tss.tss_esp0 pointer */
	movl	%esp, _common_tss + TSS_ESP0

	movl	$0, %esi
	btrl	%esi, _private_tss
	jae	1f

	movl	$_common_tssd, %edi

	/* move correct tss descriptor into GDT slot, then reload tr */
	movl	_tss_gdt, %ebx			/* entry in GDT */
	movl	0(%edi), %eax
	movl	%eax, 0(%ebx)
	movl	4(%edi), %eax
	movl	%eax, 4(%ebx)
	movl	$GPROC0_SEL*8, %esi		/* GSEL(entry, SEL_KPL) */
	ltr	%si
1:

	sti

	/*
	 * XXX callers of cpu_switch() do a bogus splclock().  Locking should
	 * be left to cpu_switch().
	 */
	call	_spl0

	ALIGN_TEXT
idle_loop:
	cli
	call	_procrunnable
	testl	%eax,%eax
	CROSSJUMP(jnz, sw1a, jz)
#ifdef	DEVICE_POLLING
	call	_idle_poll
#else	/* standard code */
	call	_vm_page_zero_idle
#endif
	testl	%eax, %eax
	jnz	idle_loop
	call	*_hlt_vector			/* wait for interrupt */
	jmp	idle_loop

#endif /* SMP */

CROSSJUMPTARGET(_idle)

#if 0

ENTRY(default_halt)
	sti
#ifndef SMP
	hlt					/* XXX:	 until a wakeup IPI */
#endif
	ret

#endif

/*
 * cpu_switch()
 */
ENTRY(cpu_switch)
	
	/* switch to new process. first, save context as needed */
	movl	_curproc,%ecx

	/* if no process to save, don't bother */
	testl	%ecx,%ecx
	je	sw1

#ifdef SMP
	movb	P_ONCPU(%ecx), %al		/* save "last" cpu */
	movb	%al, P_LASTCPU(%ecx)
	movb	$0xff, P_ONCPU(%ecx)		/* "leave" the cpu */
#endif /* SMP */
	movl	P_VMSPACE(%ecx), %edx
#ifdef SMP
	movl	_cpuid, %eax
#else
	xorl	%eax, %eax
#endif /* SMP */
	btrl	%eax, VM_PMAP+PM_ACTIVE(%edx)

	movl	P_ADDR(%ecx),%edx

	movl	(%esp),%eax			/* Hardware registers */
	movl	%eax,PCB_EIP(%edx)
	movl	%ebx,PCB_EBX(%edx)
	movl	%esp,PCB_ESP(%edx)
	movl	%ebp,PCB_EBP(%edx)
	movl	%esi,PCB_ESI(%edx)
	movl	%edi,PCB_EDI(%edx)
	movl	%gs,PCB_GS(%edx)

	/* test if debug regisers should be saved */
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
	movl	_mp_lock, %eax
	/* XXX FIXME: we should be saving the local APIC TPR */
#ifdef DIAGNOSTIC
	cmpl	$FREE_LOCK, %eax		/* is it free? */
	je	badsw4				/* yes, bad medicine! */
#endif /* DIAGNOSTIC */
	andl	$COUNT_FIELD, %eax		/* clear CPU portion */
	movl	%eax, PCB_MPNEST(%edx)		/* store it */
#endif /* SMP */

#if NNPX > 0
	/* have we used fp, and need a save? */
	cmpl	%ecx,_npxproc
	jne	1f
	addl	$PCB_SAVEFPU,%edx		/* h/w bugs make saving complicated */
	pushl	%edx
	call	_npxsave			/* do it in a big C function */
	popl	%eax
1:
#endif	/* NNPX > 0 */

	movl	$0,_curproc			/* out of process */

	/* save is done, now choose a new process or idle */
sw1:
	cli

#ifdef SMP
	/* Stop scheduling if smp_active goes zero and we are not BSP */
	cmpl	$0,_smp_active
	jne	1f
	cmpl	$0,_cpuid
	CROSSJUMP(je, _idle, jne)		/* wind down */
1:
#endif

sw1a:
	call	_chooseproc			/* trash ecx, edx, ret eax*/
	testl	%eax,%eax
	CROSSJUMP(je, _idle, jne)		/* if no proc, idle */
	movl	%eax,%ecx

	xorl	%eax,%eax
	andl	$~AST_RESCHED,_astpending

#ifdef	DIAGNOSTIC
	cmpl	%eax,P_WCHAN(%ecx)
	jne	badsw1
	cmpb	$SRUN,P_STAT(%ecx)
	jne	badsw2
#endif

	movl	P_ADDR(%ecx),%edx

#if defined(SWTCH_OPTIM_STATS)
	incl	_swtch_optim_stats
#endif
	/* switch address space */
	movl	%cr3,%ebx
	cmpl	PCB_CR3(%edx),%ebx
	je	4f
#if defined(SWTCH_OPTIM_STATS)
	decl	_swtch_optim_stats
	incl	_tlb_flush_count
#endif
	movl	PCB_CR3(%edx),%ebx
	movl	%ebx,%cr3
4:

#ifdef SMP
	movl	_cpuid, %esi
#else
	xorl	%esi, %esi
#endif
	cmpl	$0, PCB_EXT(%edx)		/* has pcb extension? */
	je	1f
	btsl	%esi, _private_tss		/* mark use of private tss */
	movl	PCB_EXT(%edx), %edi		/* new tss descriptor */
	jmp	2f
1:

	/* update common_tss.tss_esp0 pointer */
	movl	%edx, %ebx			/* pcb */
	addl	$(UPAGES * PAGE_SIZE - 16), %ebx
	movl	%ebx, _common_tss + TSS_ESP0

	btrl	%esi, _private_tss
	jae	3f
#ifdef SMP
	movl	$gd_common_tssd, %edi
	addl	%fs:0, %edi
#else
	movl	$_common_tssd, %edi
#endif
2:
	/* move correct tss descriptor into GDT slot, then reload tr */
	movl	_tss_gdt, %ebx			/* entry in GDT */
	movl	0(%edi), %eax
	movl	%eax, 0(%ebx)
	movl	4(%edi), %eax
	movl	%eax, 4(%ebx)
	movl	$GPROC0_SEL*8, %esi		/* GSEL(entry, SEL_KPL) */
	ltr	%si
3:
	movl	P_VMSPACE(%ecx), %ebx
#ifdef SMP
	movl	_cpuid, %eax
#else
	xorl	%eax, %eax
#endif
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
	movl	$0, lapic_tpr
#else
	andl	$~APIC_TPR_PRIO, lapic_tpr
#endif /** CHEAP_TPR */
#endif /** GRAB_LOPRIO */
	movl	_cpuid,%eax
	movb	%al, P_ONCPU(%ecx)
#endif /* SMP */
	movl	%edx, _curpcb
	movl	%ecx, _curproc			/* into next process */

#ifdef SMP
	movl	_cpu_lockid, %eax
	orl	PCB_MPNEST(%edx), %eax		/* add next count from PROC */
	movl	%eax, _mp_lock			/* load the mp_lock */
	/* XXX FIXME: we should be restoring the local APIC TPR */
#endif /* SMP */

#ifdef	USER_LDT
	cmpl	$0, PCB_USERLDT(%edx)
	jnz	1f
	movl	__default_ldt,%eax
	cmpl	_currentldt,%eax
	je	2f
	lldt	__default_ldt
	movl	%eax,_currentldt
	jmp	2f
1:	pushl	%edx
	call	_set_user_ldt
	popl	%edx
2:
#endif

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
	movl	%dr7,%eax                /* load dr7 so as not to disturb */
	andl    $0x0000fc00,%eax         /*   reserved bits               */
	pushl   %ebx
	movl    PCB_DR7(%edx),%ebx
	andl	$~0x0000fc00,%ebx
	orl     %ebx,%eax
	popl	%ebx
	movl    %eax,%dr7
1:

	sti
	ret

CROSSJUMPTARGET(sw1a)

#ifdef DIAGNOSTIC
badsw1:
	pushl	$sw0_1
	call	_panic

sw0_1:	.asciz	"cpu_switch: has wchan"

badsw2:
	pushl	$sw0_2
	call	_panic

sw0_2:	.asciz	"cpu_switch: not SRUN"
#endif

#if defined(SMP) && defined(DIAGNOSTIC)
badsw4:
	pushl	$sw0_4
	call	_panic

sw0_4:	.asciz	"cpu_switch: do not have lock"
#endif /* SMP && DIAGNOSTIC */

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

#if NNPX > 0
	/*
	 * If npxproc == NULL, then the npx h/w state is irrelevant and the
	 * state had better already be in the pcb.  This is true for forks
	 * but not for dumps (the old book-keeping with FP flags in the pcb
	 * always lost for dumps because the dump pcb has 0 flags).
	 *
	 * If npxproc != NULL, then we have to save the npx h/w state to
	 * npxproc's pcb and copy it to the requested pcb, or save to the
	 * requested pcb and reload.  Copying is easier because we would
	 * have to handle h/w bugs for reloading.  We used to lose the
	 * parent's npx state for forks by forgetting to reload.
	 */
	movl	_npxproc,%eax
	testl	%eax,%eax
	je	1f

	pushl	%ecx
	movl	P_ADDR(%eax),%eax
	leal	PCB_SAVEFPU(%eax),%eax
	pushl	%eax
	pushl	%eax
	call	_npxsave
	addl	$4,%esp
	popl	%eax
	popl	%ecx

	pushl	$PCB_SAVEFPU_SIZE
	leal	PCB_SAVEFPU(%ecx),%ecx
	pushl	%ecx
	pushl	%eax
	call	_bcopy
	addl	$12,%esp
#endif	/* NNPX > 0 */

1:
	ret
