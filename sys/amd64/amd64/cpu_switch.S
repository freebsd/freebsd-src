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
#include <machine/apic.h>
#include <machine/smptests.h>		/** GRAB_LOPRIO */
#include <machine/lock.h>
#endif /* SMP */

#include "assym.s"


/*****************************************************************************/
/* Scheduling                                                                */
/*****************************************************************************/

	.data

	.globl	_panic

#if defined(SWTCH_OPTIM_STATS)
	.globl	_swtch_optim_stats, _tlb_flush_count
_swtch_optim_stats:	.long	0		/* number of _swtch_optims */
_tlb_flush_count:	.long	0
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
	
	/* switch to new process. first, save context as needed */
	movl	_curproc,%ecx

	/* if no process to save, don't bother */
	testl	%ecx,%ecx
	jz	sw1

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

	/* test if debug registers should be saved */
	movb    PCB_FLAGS(%edx),%al
	andb    $PCB_DBREGS,%al
	jz      1f                              /* no, skip over */
	movl    %dr7,%eax                       /* yes, do the save */
	movl    %eax,PCB_DR7(%edx)
	andl    $0x0000ff00, %eax               /* disable all watchpoints */
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
 
	/* save sched_lock recursion count */
	movl	_sched_lock+MTX_RECURSE,%eax
	movl    %eax,PCB_SCHEDNEST(%edx)
 
#ifdef SMP
	/* XXX FIXME: we should be saving the local APIC TPR */
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

	/* save is done, now choose a new process */
sw1:

#ifdef SMP
	/* Stop scheduling if smp_active goes zero and we are not BSP */
	cmpl	$0,_smp_active
	jne	1f
	cmpl	$0,_cpuid
	je	1f

	movl	_idleproc, %eax
	jmp	sw1b
1:
#endif

	/*
	 * Choose a new process to schedule.  chooseproc() returns idleproc
	 * if it cannot find another process to run.
	 */
sw1a:
	call	_chooseproc			/* trash ecx, edx, ret eax*/

#ifdef INVARIANTS
	testl	%eax,%eax			/* no process? */
	jz	badsw3				/* no, panic */
#endif
sw1b:
	movl	%eax,%ecx

	xorl	%eax,%eax
	andl	$~AST_RESCHED,_astpending

#ifdef	INVARIANTS
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
	movl    PCB_DR7(%edx),%eax
	movl    %eax,%dr7
1:

	/*
	 * restore sched_lock recursion count and transfer ownership to
	 * new process
	 */
	movl	PCB_SCHEDNEST(%edx),%eax
	movl	%eax,_sched_lock+MTX_RECURSE

	movl	_curproc,%eax
	movl	%eax,_sched_lock+MTX_LOCK

	ret

CROSSJUMPTARGET(sw1a)

#ifdef INVARIANTS
badsw2:
	pushl	$sw0_2
	call	_panic

sw0_2:	.asciz	"cpu_switch: not SRUN"

badsw3:
	pushl	$sw0_3
	call	_panic

sw0_3:	.asciz	"cpu_switch: chooseproc returned NULL"
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
